# MSPM0L1306 初始调试工程

这是一个用于 MSPM0L1306 核心板的 CCS 20.2 工程，包含两个初始功能：

- PA3 上的 LED 每 500 ms 翻转一次。
- UART0 使用 PA8（TX）和 PA9（RX），以 115200 8-N-1 配置回显接收到的字节。

## 工具

- CCS：20.2，或 CCS 工程声明的兼容版本
- MSPM0 SDK：2.11.00.07
- SysConfig：1.26.2
- 调试探针：Horco CMSIS-DAP v2
- PC 串口：使用本机分配给开发板的 COM 端口

CCS 用于导入工程、编辑 SysConfig 和构建。Horco 探针是通用的 CMSIS-DAP v2
探针，CCS 20.2 无法直接将其识别为 XDS 调试探针，因此使用 pyOCD 进行 Flash
烧录和 SWD 调试。

## 接线

| DAPLink 信号 | 核心板信号 |
| --- | --- |
| GND | GND |
| SWDIO | PA19 / SWDIO |
| SWCLK | PA20 / SWCLK |
| TX | PA9 / UART0_RX |
| RX | PA8 / UART0_TX |

使用 3.3 V 逻辑电平。核心板只能选择一个电源供电。

## 调试工具

在 PowerShell 中执行一次：

```powershell
.\tools\install-debug-tools.ps1
```

在仓库根目录运行 `tools\install-debug-tools.ps1`。该脚本会创建
`tools\.venv`、安装 pyOCD，并将 TI 的 MSPM0L11XX/L13XX CMSIS-Pack 下载到
`tools\packs`。该 Pack 包含 MSPM0L1306 的 Flash 算法。

## CCS 工作流程

1. 启动 CCS，并选择一个位于仓库外部的 workspace。
2. 选择 `Project -> Import CCS Projects`，然后选择本仓库目录。
3. 打开 `mspm0l1306_bringup.syscfg`，查看或修改图形化配置。
4. 构建 Debug 配置。
5. 运行 `..\tools\flash-and-debug.ps1 -Action Load`，擦除并烧录 ELF。

## VS Code 工作流程

在 VS Code 中打开本仓库目录。CCS 仍负责 SysConfig 图形化编辑和工程导入，
VS Code 用于日常编辑、构建和 SWD 调试。

1. 运行 `MSPM0: Build`，构建
   `Debug\mspm0l1306_bringup.out`。
2. 运行 `MSPM0: List pyOCD probes`，确认 Horco CMSIS-DAP 探针已被识别。
3. 运行 `MSPM0: Start pyOCD GDB server`，保持该任务终端运行。
4. 按 F5 启动 `MSPM0L1306: Attach to pyOCD`。该配置会先构建，然后连接到
   `localhost:3333` 上的 pyOCD GDB 服务。

`MSPM0: Load firmware to Flash (erases/writes target)` 被设计为独立任务。
它会擦除并写入目标板 Flash；构建、探针列表和 GDB 服务任务都不会写入 Flash。

如果列表中没有探针，请检查 USB 线、目标板供电、SWDIO/PA19、SWCLK/PA20 的
接线以及共地连接。如果烧录失败，请先重新构建，并确认目标板使用 3.3 V 逻辑电平。
启动 F5 调试前，必须先运行 GDB server 任务；F5 配置只连接到
`localhost:3333`，不会写入 Flash。

## 串口测试

使用本机分配给开发板的 COM 端口，配置为 115200 波特率、8 位数据、无校验、1 位
停止位、无流控。发送 `abc123` 后，终端应当准确收到 `abc123`。
