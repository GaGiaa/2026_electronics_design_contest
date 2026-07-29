# 工程依赖说明

## STM32H723 JY901S

`stm32h723_app/` 的新增运行时硬件依赖为维特智能 JY901S 九轴姿态模块。模块使用 UART 二进制回传，当前配置为 `234000 bit/s`、`200 Hz`，接线为模块 TX 至 H723 `PG0/UART9_RX`，并与板端共地；`PG1/UART9_TX` 仅为后续模块配置保留。本工程不引入该模块的第三方软件库，协议解析为仓库内纯 C 实现。

本文档说明本仓库在 Windows 环境下进行构建、开发、调试和烧录所需的软硬件依赖。

本文档是软件版本、工具链和工程依赖的唯一权威来源。跨电脑安装和构建步骤见
`docs/SETUP.md`，AI 交接规则和当前验证状态见 `docs/AI_HANDOFF.md`，C 代码和文档写作
规范见 `docs/CODING_STYLE.md`，项目地图见根目录 `README.md`。

## 1. 基础软件环境

- Windows
- PowerShell
- Git
- Python 3：用于 pyOCD 和调试工具
- VS Code：可选，用于脚本任务、代码浏览和调试配置

## 2. TI CCS 构建依赖

CCS、MSPM0 SDK、SysConfig 和 TI Arm Clang 的固定版本见第 9 节“版本基线”。

MSPM0 SDK 提供以下工程依赖：

- MSPM0G3507 和 MSPM0L1306 设备头文件；
- DriverLib；
- 启动文件；
- linker 文件；
- FreeRTOS 内核源码；
- TI Arm Clang 的 FreeRTOS 移植层。

`mspm0g3507_app` 的完整应用构建应使用仓库脚本：

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-app.ps1
```

CCS 工程可用于源码浏览、SysConfig 和调试配置。直接点击 CCS 的
`Project -> Build Project` 还需要在本机手动配置 SDK FreeRTOS 源文件和包含路径，
否则会出现 `FreeRTOS.h`、`pid.h` 找不到或 FreeRTOS 内核符号未定义。不要将本机
绝对路径写入仓库；跨电脑构建应使用上面的 PowerShell 脚本和环境变量配置。

## 3. Keil 构建依赖

- Keil MDK、Arm Compiler、MSPM0 SDK 和 SysConfig 的固定版本见第 9 节“版本基线”；
- 还需要安装 MSPM0G3507 对应 CMSIS-Pack。

Keil 工程使用：

- SDK 中的 FreeRTOS 内核；
- 仓库内的 `keil/mspm0g3507_app/freertos_port/ARM_CM0` 移植层；
- 仓库内的应用源码和 PID 源码。

`keil/mspm0g3507_app/mspm0g3507_app.uvprojx` 是模板，不能直接作为已经配置好的
本机工程使用。构建或打开工程前，应先生成：

```text
keil/mspm0g3507_app/mspm0g3507_app.local.uvprojx
```

生成的 `.local.uvprojx` 使用当前电脑的 `MSPM0_SDK_ROOT`，并且已经被 Git 忽略。

## 4. 调试与烧录依赖

- CMSIS-DAP 调试器；
- pyOCD：`>=0.45,<0.46`；
- MSPM0G3507 对应 CMSIS-Pack；
- Arm GNU GDB；
- `ARM_GDB_PATH` 指向 `arm-none-eabi-gdb.exe`。

这些工具用于：

- GDB 调试；
- 连接目标板；
- 烧录和 Flash 操作。

普通编译、SysConfig 生成和静态检查不会擦除或写入 Flash。Flash 操作必须通过单独
的调试/烧录命令执行，并在操作前确认目标板和授权范围。

## 5. 环境变量

推荐配置以下环境变量：

```text
MSPM0_SDK_ROOT
SYSCONFIG_ROOT
TI_ARM_CLANG
CCS_ROOT
KEIL_ROOT
ARM_GDB_PATH
```

可以在仓库根目录运行以下脚本自动搜索常见安装位置，并保存到当前 Windows 用户环境：

```powershell
powershell -ExecutionPolicy Bypass -File tools\configure-toolchain.ps1 -PersistUserEnvironment
```

也可以在当前 PowerShell 会话中手动设置，或作为构建脚本参数传入。构建脚本参数优先于
环境变量。不要把这些变量的本机绝对值提交到 Git。

## 6. 工程自身的代码依赖

工程源码依赖以下组件：

- MSPM0 SDK DriverLib；
- SDK FreeRTOS Kernel；
- SDK TI Arm Clang FreeRTOS port；
- 本地 `FreeRTOSConfig.h`；
- 本地 `shared/pid` 纯 C PID 模块；
- SysConfig 生成的 `ti_msp_dl_config.c` 和 `ti_msp_dl_config.h`；
- CMSIS Core headers。

项目没有依赖外部 MotorLib 的完整代码，只保留了仓库内的 PID 核心代码。

G3507 应用源码按以下职责分层：`app/` 负责应用启动、任务和共享状态；`drivers/` 负责
板级外设；`algorithms/` 负责可复用算法；`protocols/` 负责 CRSF 和 VOFA；`services/`
负责可复用服务；`config/` 负责配置；`platform/` 负责芯片相关中断分发。每个实现文件
只在 CCS 和 Keil 工程中加入一次，根目录旧头文件只作为兼容 include 入口。

## 7. 工程依赖矩阵

| 工程 | 主要源码依赖 | 构建入口 |
| --- | --- | --- |
| `mspm0l1306_bringup/` | MSPM0L1306 DriverLib、SysConfig 生成文件、L1306 linker | `tools/build-mspm0l1306.ps1` |
| `mspm0g3507_bringup/` | MSPM0G3507 DriverLib、SysConfig 生成文件、G3507 linker | `tools/build-mspm0g3507.ps1` |
| `mspm0g3507_freertos/` | G3507 DriverLib、SDK FreeRTOS Kernel、工程 `FreeRTOSConfig.h` | `tools/build-mspm0g3507-freertos.ps1` |
| `mspm0g3507_app/` | G3507 DriverLib、SDK FreeRTOS Kernel、TI Arm Clang port、共享 `shared/pid` 和分层应用源码 | `tools/build-mspm0g3507-app.ps1` |
| `stm32h723_app/` | STM32Cube FW_H7、CubeMX 生成代码、FreeRTOS、共享 `shared/pid` 和 Keil MDK-ARM | `stm32h723_app/MDK-ARM/stm32h723_app.uvprojx` |
| `keil/mspm0g3507_app/` | Keil Arm Compiler、SDK FreeRTOS Kernel、本地 `freertos_port/ARM_CM0` 和共享 G3507 应用源码 | `tools/build-keil-mspm0g3507-app.ps1` |

## 8. 运行时硬件依赖

G3507 主应用依赖以下实际硬件：

- MSPM0G3507 核心板；
- 四路电机和电机驱动器；
- 四路 AB 编码器；
- BMI160 IMU；
- 灰度传感器；
- WS2812 LED；
- CRSF 接收机，可选；
- 与工程 SysConfig 配置匹配的 UART、SPI、PWM、定时器等外设连接。

硬件引脚、协议和外设分配以 `mspm0g3507_app/mspm0g3507_app.syscfg`、应用 README
以及交接文档为准。更换核心板、芯片型号或 SysConfig 工程时，不得混用 G3507 和
L1306 的设备包、启动文件、linker、SysConfig 或引脚配置。

## 9. 版本基线

构建时应尽量固定使用以下版本组合：

| 依赖 | 版本 |
| --- | --- |
| MSPM0 SDK | 2.11.00.07 |
| SysConfig | 1.26.2 |
| TI Arm Clang | 4.0.3.LTS |
| Code Composer Studio | 20.2 或兼容版本 |
| Keil MDK | 5.43a |
| Arm Compiler | 6.24 |
| pyOCD | `>=0.45,<0.46` |

## 10. STM32H723 应用基础工程

`stm32h723_app/` 使用以下本地工具链。`.ioc` 和 CubeMX 生成的源码纳入仓库；Keil 构建产物不纳入仓库。

| 依赖 | 版本或用途 |
| --- | --- |
| STM32CubeMX | 6.15.0；图形化外设配置和代码生成 |
| STM32Cube FW_H7 | V1.12.1；STM32H723 HAL、CMSIS 和 FreeRTOS middleware |
| Keil MDK-ARM | 本机 `D:\Keil_v5\UV4\UV4.exe`；构建 `stm32h723_app/MDK-ARM/stm32h723_app.uvprojx` |

应安装生成 Keil 工程所需的 STM32H7 CMSIS-Pack。只允许从 `stm32h723_app/stm32h723_app.ioc` 重新生成 H7 外设代码；不得把 H7 的 HAL、启动文件、linker 或 FreeRTOS 文件混入 MSPM0 工程。

版本不一致时，首先检查 CCS Build Console 或构建脚本实际解析出的路径，尤其关注
SDK、SysConfig 和 TI Arm Clang 的版本号。构建输出出现旧 SDK 路径时，应重新配置
环境变量或在全新的 CCS workspace 中重新导入工程。
