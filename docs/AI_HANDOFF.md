# MSPM0L1306 AI 工程交接文档

## 强制工作流程

任何 AI 在查看、修改、构建、调试或烧录本仓库前，必须完整阅读本文件。
每次开发任务结束、交接给用户前，必须更新本文件，记录实际修改内容、已执行的验证、发现的硬件或工具事实，以及遗留风险。仓库根目录的 `AGENTS.md` 同样约束此规则。

每次 Git 提交必须包含简洁标题和详细正文。正文需写明功能变化、涉及的工具或硬件，以及实际验证证据；禁止只有一行标题的提交。

## 工程现状

- 目标芯片：MSPM0L1306，VQFN-32（RHB），TI MSPM0 SDK 2.05.01.00。
- 当前功能：PA3 LED 输出每 500 ms 翻转；UART0 通过 RX 中断和 64 字节、具有中断保护的 FIFO 回显接收数据。
- SysConfig 源文件：`mspm0l1306_bringup.syscfg`。
- 固件入口：`app/main.c`。
- GPIO 板级封装：`board/board_led.c`。
- UART 板级封装：`board/board_uart.c`。

## 引脚分配与接线

| 功能 | MCU 引脚 | 与 DAPLink 的连接 |
| --- | --- | --- |
| LED 输出 | PA3 | 板载 LED 电路 |
| UART0 TX | PA8 | DAPLink RX |
| UART0 RX | PA9 | DAPLink TX |
| SWDIO | PA19 | DAPLink SWDIO |
| SWCLK | PA20 | DAPLink SWCLK |
| 地 | GND | DAPLink GND |

使用 3.3 V 逻辑电平。核心板不能同时由多个电源供电。DAPLink 虚拟串口当前为 `COM37`，使用前必须重新确认串口号。

## 工具链

- CCS：`D:\Software\ti\ccs2020`（20.2.0.00012）。只用于 SysConfig 图形化编辑和可选的工程导入。
- MSPM0 SDK：`D:\Software\ti\mspm0_sdk_2_05_01_00`（2.05.01.00）。
- TI Clang：`D:\Software\ti\ccs2020\ccs\tools\compiler\ti-cgt-armllvm_4.0.3.LTS\bin\tiarmclang.exe`。
- GNU Make：`D:\Software\ti\ccs2020\ccs\utils\bin\gmake.exe`。
- ARM GDB：`D:\Software\STM32CubeCLT_1.18.0\GNU-tools-for-STM32\bin\arm-none-eabi-gdb.exe`。
- pyOCD：工程本地 `.venv`，版本 0.45.0。
- 调试器：Horco CMSIS-DAP v2，UID `ad000b52`。

## 构建、调试与烧录

必须在工程根目录执行以下命令构建：

```powershell
& 'D:\Software\ti\ccs2020\ccs\utils\bin\gmake.exe' -C ticlang all
```

SysConfig 生成文件必须位于 `ticlang` 根目录，不能移回 `ticlang\syscfg`。

标准 pyOCD 操作只使用 `tools/flash-and-debug.ps1`：

```powershell
powershell -ExecutionPolicy Bypass -File tools\flash-and-debug.ps1 -Action List
powershell -ExecutionPolicy Bypass -File tools\flash-and-debug.ps1 -Action GdbServer
powershell -ExecutionPolicy Bypass -File tools\flash-and-debug.ps1 -Action Load
```

`Load` 可能擦除和写入目标 Flash，执行前必须明确告知并获得用户同意。`List`、`Build` 和 `GdbServer` 均不会烧录 Flash。

TI Clang 输出的是扩展名为 `.out` 的 ELF32 文件。pyOCD 不会根据 `.out` 自动识别 ELF，因此烧录包装脚本必须保留 `--format elf`。

TI CMSIS-Pack 要求对 AP0 ROM table 应用 datapatch，但 pyOCD 0.45.0 不会自动应用它。`tools/pyocd-mspm0l1306.py` 会在 AP 创建后注入 `0xF0000000`；`GdbServer` 和 `Load` 必须继续保留 `--script` 参数。

## VS Code

使用已跟踪的 `.vscode` 配置：

- `MSPM0: Build`：通过 TI gmake 构建。
- `MSPM0: List pyOCD probes`：非破坏性枚举探针。
- `MSPM0: Start pyOCD GDB server`：启动外部调试服务器。
- `MSPM0: Load firmware to Flash (erases/writes target)`：烧录 Flash。
- F5 调试配置：`MSPM0L1306: Attach to pyOCD`。

## 已验证内容

- FIFO 主机测试：`powershell -ExecutionPolicy Bypass -File tests\run_host_tests.ps1`，4 项全部通过。
- LED 引脚配置测试：`powershell -ExecutionPolicy Bypass -File tests\test_led_pin_config.ps1`，确认 PA3。
- pyOCD 兼容脚本测试：`.\.venv\Scripts\python.exe tests\test_pyocd_mspm0l1306_script.py`，通过。
- 烧录包装器测试：`powershell -ExecutionPolicy Bypass -File tests\test_flash_wrapper.ps1`，确认存在 `--format elf`。
- 用户已在硬件上确认：DAPLink 可烧录、PA3 可翻转、UART0 虚拟串口回显正常。

## Git 与文件卫生

不要把以下生成或本地文件提交到 Git：`.venv/`、`tools/packs/`、`tools/*.log`、`ticlang/*.obj`、`ticlang/*.out`、SysConfig 生成文件和 `tests/build/`。完整规则以 `.gitignore` 为准。

Git 仓库已初始化在 `main` 分支。根提交记录了 PA3 LED、UART 回显、VS Code、pyOCD 兼容与验证状态；提交信息政策已经写入根提交。
