# pyOCD 调试工作归档总结

## 归档目的

本文档记录 MSPM0G3507 App 工程在归档到 `archive/pyocd_debug` 前已经完成的功能、调试工具改动、遇到的问题和当前验证状态。

本文档编写日期：2026-07-22。

归档前记录所在分支：`develop_1`。

归档前 HEAD：`18c5c3c 校准编码器轮位映射与方向`。

归档目标分支：`archive/pyocd_debug`。

## 已取得的成果

### MSPM0G3507 App 功能

- 建立独立的 MSPM0G3507 FreeRTOS App 工程，并接入 TI SysConfig 生成代码。
- 完成四路双 PWM 电机驱动，包含停止、正转、反转和占空比限制。
- 完成四路正交编码器采集。A 相使用 GPIOA 双边沿中断，B 相用于判断方向。
- 完成编码器累计计数、采样周期计数和速度遥测数据结构。
- 完成编码器遥测任务，按 100 ms 周期通过 UART0 发送四个车轮的数据。
- UART 接收使用中断和 FreeRTOS 队列，发送使用独立的串行发送任务，避免多个任务直接抢占 UART。
- 完成 WS2812 四灯珠 SPI 输出和基础动画任务。
- 增加 PA2 无源蜂鸣器 PWM 驱动。蜂鸣器功能默认关闭，启用后默认参数为 2 kHz、50% 占空比、鸣叫 200 ms、停止 1800 ms。
- 保留静态 FreeRTOS 任务、队列和栈分配，避免依赖运行时堆分配。

### 编码器轮位和方向校准

编码器引脚映射已经按实测结果转移到 `mspm0g3507_app/mspm0g3507_app.syscfg` 中维护。最终生成的逻辑轮位如下：

| 逻辑轮位 | A 相 | B 相 | 软件方向符号 |
| --- | --- | --- | --- |
| 前左 FL | PA15 | PB24 | `+1` |
| 前右 FR | PA17 | PA22 | `-1` |
| 后左 RL | PA14 | PA9 | `+1` |
| 后右 RR | PA16 | PB20 | `-1` |

`board_encoder.c` 使用 `ENCODER_*` 生成宏和方向符号，不再在驱动中硬编码物理引脚到逻辑轮位的替换关系。这样重新生成 SysConfig 代码后，映射仍然有唯一来源。

当前计数参数为：

- 编码器标称每转 520 个计数；软件对 A 相上升沿和下降沿都计数，因此默认每机械转 `1040` 个计数。
- 参考轮径为 48 mm。
- 编码器采样周期为 10 ms。
- 速度计算公式为：

  `speed_mm_per_s = delta_counts * wheel_diameter_mm * pi * 1000 / (counts_per_revolution * sample_period_ms)`

`1040` 仍是默认值，尚未用实测物理转数完成每圈计数精校准。

## pyOCD 调试改动

原有 Cortex-Debug 直接启动 pyOCD 的方式不稳定，当前 App 配置改为连接外部 GDB 目标：

- GDB 目标：`localhost:3333`。
- 目标芯片：`MSPM0G3507`。
- CMSIS-DAP/DAPLink 探针 UID：`2dd0719d`。
- SWD 频率：1 MHz。
- Telnet 端口：`4444`。
- 使用 `tools/.venv` 中的 `python -m pyocd gdbserver` 启动服务。
- 使用 TI Pack：`tools/packs/TexasInstruments.MSPM0G1X0X_G3X0X_DFP.1.3.1.pack`。
- 调试启动流程不写入 Flash，只连接当前已烧录程序。
- VS Code 绿色调试按钮会先执行 App 构建和启动 pyOCD，再连接 GDB；结束调试时执行退出脚本。

相关文件：

- `.vscode/launch.json`：App 的 Cortex-Debug attach 配置、自动启动任务和退出任务。
- `.vscode/tasks.json`：构建、启动 pyOCD、等待端口就绪和停止 pyOCD 的任务定义。
- `tools/start-mspm0g3507-app-debug.ps1`：以稳定参数启动前台 pyOCD GDB server。
- `tools/stop-mspm0g3507-app-debug.ps1`：优先通过 GDB `monitor exit` 优雅关闭服务，必要时才清理残留进程。
- `tests/test_mspm0g3507_app.ps1`：增加对调试脚本、探针 UID、端口和退出流程的静态检查。
- `docs/AI_HANDOFF.md`：补充自动 pyOCD 调试流程和验证记录。

普通烧录仍通过 `tools/flash-and-debug-g3507.ps1 -Action Load -Firmware app` 执行；该流程与只读 Flash 的调试 attach 流程分开。

## 遇到过的问题及处理

### CCS 无法识别通用 CMSIS-DAP

Horco CMSIS-DAP 探针不会被 CCS 20.2 识别为 XDS110，因此不能依赖 CCS 的 XDS110 调试链路。最终采用 pyOCD GDB server 加 ARM GDB，再由 Cortex-Debug 连接。

### Pack 文件被占用

同时启动多个 pyOCD GDB server 时，TI Pack 文件会被其他进程锁定，安装脚本报出“文件正由另一进程使用”。处理方式是确保同一时间只有一个 G3507 pyOCD server，并在启动脚本中检查 GDB 端口 `3333` 是否已被占用。

### Cortex-Debug 重复追加 gdbserver

Cortex-Debug 1.12.1 会在 `serverpath` 后追加 `gdbserver`。如果直接把 `pyocd-gdbserver.exe` 设置为 server path，就会形成类似 `pyocd-gdbserver.exe gdbserver ...` 的无效命令。当前脚本改用 `python -m pyocd gdbserver`，并让 Cortex-Debug 只连接外部 GDB 目标。

### 虚拟环境移动后启动器失效

pyOCD 虚拟环境最初在其他目录创建，移动到当前仓库后，虚拟环境中的控制台启动器仍指向旧路径。重新安装固定版本 `pyocd==0.45.0` 后恢复了正确的入口。当前约束为 `pyocd>=0.45,<0.46`。

### 强制结束 pyOCD 导致探针异常

直接强制结束 pyOCD 后，CMSIS-DAP HID 端点可能处于异常状态，表现为 `Timeout reading from probe 2dd0719d` 或 pyUSB 后端断言失败。曾通过重置 USB PnP 实例 `USB\VID_FAED&PID_4873\2DD0719D` 恢复探针。当前停止脚本优先通过 GDB `monitor exit` 关闭服务，以减少再次出现该问题的概率。

## 已完成的验证

以下静态检查和 TI Clang 构建已经通过：

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\test_mspm0g3507_app.ps1
powershell -ExecutionPolicy Bypass -File .\tools\build-mspm0g3507-app.ps1
```

静态检查输出：

```text
PASS: MSPM0G3507 app static integration checks passed.
```

硬件 pyOCD Commander 已成功识别目标：

- DP ID：`0x6ba02477`。
- Cortex-M0+ r0p1。
- AHB-AP。
- 4 个硬件断点。
- 2 个硬件观察点。

通过端口 `3333` 的 ARM GDB 批处理连接也已成功读取全局变量：

```text
g_encoder_samples address: 0x20202530
g_encoder_samples[0].delta_counts: 0
g_encoder_samples[0].total_counts: 7
g_encoder_samples[0].speed_mm_per_s: 0
```

GDB 发送 `monitor exit` 后，pyOCD GDB server 能够正常退出。

## 当前限制和待验收项

- 用户尚未在当前桌面环境中完成 VS Code 绿色调试按钮的完整交互验证，因此不能把 Watch 窗口实时观察和断点单步作为已验收结果。
- 用户尚未验证归档前这批工作树改动在其最终硬件状态下的整体运行结果。
- 编码器每圈 `1040` 仍为默认估计值，需要按已知物理转数重新测量并更新 `BOARD_ENCODER_COUNTS_PER_REVOLUTION`。
- 由于 MSPM0G3507 为 Cortex-M0+，没有硬件浮点单元；当前速度结构使用 `float`，速度计算由软件浮点库完成，后续如需降低开销可改为定点整数计算。
- pyOCD 调试依赖当前电脑上的工具链路径、Pack 文件、虚拟环境和探针 UID。换电脑、换探针或 USB 设备重新枚举后，需要更新对应配置。
- 调试启动脚本使用固定 ARM GDB 路径：`D:\Software\STM32CubeCLT_1.18.0\GNU-tools-for-STM32\bin\arm-none-eabi-gdb.exe`。
- `tools/.venv` 和 `tools/packs` 属于本地依赖目录，按仓库规则忽略，不应提交到 Git。

## 归档前注意事项

- 当前工作树中的 pyOCD 改动尚未提交；归档时应检查并提交 `.vscode`、`docs/AI_HANDOFF.md`、`tests` 和两个 pyOCD 启停脚本。
- 不要把 `tools/.venv`、`tools/packs` 或构建输出 `mspm0g3507_app/Debug` 加入提交。
- 另一个 VS Code 窗口曾显示 `develop_2` 工作树及 BMI160 相关改动。归档本工作时应确认 VS Code 当前打开的是 `D:\desktop\2026_electronics_design_contest` 对应的 `develop_1` 工作树，避免把另一工作树的改动混入归档。
- 归档后建议再次执行静态测试、TI Clang 构建、`git status --short --branch` 和 `git log --oneline --graph -6`，确认工作树干净且历史完整。
