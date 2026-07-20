# MSPM0L1306 AI 工程交接文档

## 强制工作流程

任何 AI 在查看、修改、构建、调试或烧录本仓库前，必须完整阅读本文件。
每次开发任务结束、交接给用户前，必须更新本文件，记录实际修改内容、已执行的验证、发现的硬件或工具事实，以及遗留风险。

每次 Git 提交必须包含简洁标题和详细正文。正文需写明功能变化、涉及的工具或硬件，以及实际验证证据；禁止只有一行标题的提交。

## 工程现状

- 目标芯片：MSPM0L1306，VQFN-32（RHB）。
- 当前工程：CCS 20.2 管理的 `mspm0l1306_bringup`，Debug 配置使用 TI Clang 4.0.3.LTS、MSPM0 SDK 2.11.00.07 和 SysConfig 1.26.2。
- 当前功能：PA3 LED 每 500 ms 翻转；UART0 在 PA8（TX）和 PA9（RX）以 115200、8-N-1 接收并回显数据。
- SysConfig 源文件：`mspm0l1306_bringup/mspm0l1306_bringup.syscfg`。
- 固件入口：`mspm0l1306_bringup/main.c`；LED、UART 与 FIFO 实现位于同目录下的 `board_*.c` 和 `echo_queue.c`。
- Debug ELF：`mspm0l1306_bringup/Debug/mspm0l1306_bringup.out`。`Debug/` 是 CCS 生成目录，不得提交。

旧的顶层 `app/`、`board/`、`ticlang/`、`tests/` 及 `.projectspec` 结构已由本次迁移移除；后续修改必须以 `mspm0l1306_bringup/` 中的 CCS 工程为唯一源码和构建入口。

## 引脚分配与接线

| 功能 | MCU 引脚 | 与 DAPLink 的连接 |
| --- | --- | --- |
| LED 输出 | PA3 | 板载 LED 电路 |
| UART0 TX | PA8 | DAPLink RX |
| UART0 RX | PA9 | DAPLink TX |
| SWDIO | PA19 | DAPLink SWDIO |
| SWCLK | PA20 | DAPLink SWCLK |
| 地 | GND | DAPLink GND |

使用 3.3 V 逻辑电平。核心板只能由一个电源供电。DAPLink 虚拟串口曾为 `COM37`，使用前必须重新确认实际端口号。

## 工具链

- CCS：`D:\Software\ti\ccs2020`（20.2.0.00012），用于工程导入、SysConfig 图形编辑和构建。
- MSPM0 SDK：`D:\Software\ti\ccs2020\mspm0_sdk_2_11_00_07`（2.11.00.07）。
- TI Clang：`D:\Software\ti\ccs2020\ccs\tools\compiler\ti-cgt-armllvm_4.0.3.LTS\bin\tiarmclang.exe`。
- GNU Make：`D:\Software\ti\ccs2020\ccs\utils\bin\gmake.exe`。
- ARM GDB：`D:\Software\STM32CubeCLT_1.18.0\GNU-tools-for-STM32\bin\arm-none-eabi-gdb.exe`。
- pyOCD：通过 `tools/install-debug-tools.ps1` 安装到 `tools/.venv`，版本约束为 `>=0.45,<0.46`。
- 调试器：Horco CMSIS-DAP v2。CCS 20.2 不会把它识别为 XDS 调试器，烧录和 SWD 调试使用 pyOCD。

## 构建、调试与烧录

在仓库根目录构建 Debug 配置：

```powershell
& 'D:\Software\ti\ccs2020\ccs\utils\bin\gmake.exe' -C mspm0l1306_bringup\Debug all
```

也可在 VS Code 运行 `MSPM0: Build`。CCS 的 SysConfig 输出目录配置为 `Debug/`；不要手工编辑该目录中的生成文件。

首次配置 pyOCD 工具：

```powershell
powershell -ExecutionPolicy Bypass -File tools\install-debug-tools.ps1
```

标准 pyOCD 操作仅使用 `tools/flash-and-debug.ps1`：

```powershell
powershell -ExecutionPolicy Bypass -File tools\flash-and-debug.ps1 -Action List
powershell -ExecutionPolicy Bypass -File tools\flash-and-debug.ps1 -Action GdbServer
powershell -ExecutionPolicy Bypass -File tools\flash-and-debug.ps1 -Action Load
```

`Load` 会执行 `--erase chip` 并写入目标 Flash，执行前必须明确告知并获得用户同意。`List`、`MSPM0: Build` 和 `GdbServer` 不会烧录 Flash。VS Code 的 F5 配置为连接已启动的 `localhost:3333` pyOCD GDB server，不会自动写入 Flash。

TI Clang 输出的 `.out` 是 ELF32。pyOCD 不会根据扩展名自动识别它，因此 `Load` 必须保留 `--format elf`。`tools/pyocd-mspm0l1306.py` 会在 pyOCD 发现 CoreSight 前把 AP0 ROM table 地址设为 `0xF0000000`；`GdbServer` 和 `Load` 必须持续传递 `--script` 参数。

## VS Code

已跟踪的 `.vscode` 配置提供：

- `MSPM0: Build`：在 `mspm0l1306_bringup/Debug` 运行 TI gmake。
- `MSPM0: List pyOCD probes`：非破坏性枚举探针。
- `MSPM0: Start pyOCD GDB server`：启动外部调试服务器。
- `MSPM0: Load firmware to Flash`：擦除并写入 Flash。
- F5 调试配置：`MSPM0L1306: Attach to pyOCD`。

## 已验证内容

- 已检查 `mspm0l1306_bringup/.cproject`：目标为 MSPM0L1306，Debug 构建链接输出为 `mspm0l1306_bringup.out`，并配置 MSPM0 SDK 2.11.00.07、SysConfig 1.26.2 与 TI Clang 4.0.3.LTS。
- 已检查 SysConfig：PA3、UART0 PA8/PA9、PA19 SWDIO 和 PA20 SWCLK 的配置存在。
- 已检查 VS Code 任务、调试配置和 pyOCD 包装器：它们均指向新的 `mspm0l1306_bringup/Debug` 输出路径；烧录仍显式使用 `--format elf` 与 ROM table 兼容脚本。
- 2026-07-20 已执行 `& 'D:\Software\ti\ccs2020\ccs\utils\bin\gmake.exe' -C mspm0l1306_bringup\Debug all`，成功退出，报告 `mspm0l1306_bringup.out is up to date`。
- 本次未执行探针枚举、GDB server、硬件串口测试或 Flash 写入。旧目录中的主机测试脚本已移除，不能将以前的测试结果当作本工程当前版本的验证。

## Git 与文件卫生

不要提交 `mspm0l1306_bringup/Debug/`、`tools/.venv/`、`tools/packs/`、Python 缓存、VS Code 本地状态或操作系统临时文件。完整规则以 `.gitignore` 为准。

提交本次工程迁移时，应同时记录旧目录的删除和新 CCS 工程的纳入，避免只提交其中一侧导致源码或构建入口缺失。

## 2026-07-20 CCS 工程迁移

- 本次工作区将原先自定义的顶层源码与 `ticlang` 构建结构迁移为 CCS 20.2 原生工程目录 `mspm0l1306_bringup/`。源码改为直接置于工程根目录，CCS 元数据为 `.project`、`.cproject` 和 `.ccsproject`。
- `.vscode`、`.gitignore` 和 pyOCD 脚本已改用 `mspm0l1306_bringup/Debug/mspm0l1306_bringup.out`，本地 pyOCD 环境与 CMSIS-Pack 改存放在 `tools/`。
- 已执行 `git diff --check`，未发现空白错误；已执行 CCS Debug 构建且成功。未进行烧录或其他硬件写操作。
