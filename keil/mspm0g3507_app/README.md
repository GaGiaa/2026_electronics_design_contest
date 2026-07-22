# MSPM0G3507 Keil MDK 应用工程

这是一个独立的 Keil MDK 工程，对应现有的 MSPM0G3507 FreeRTOS 应用。
应用源码共享自 `../../mspm0g3507_app`；原始的 `.project`、`.cproject` 和
SysConfig 源文件仍由 CCS 工程维护。

## 前置条件

- Keil MDK 5.43a，使用 Arm Compiler 6.24
- MSPM0 SDK 2.11.00.07
- SysConfig 1.26.2
- 已安装 CMSIS-Pack：`TexasInstruments.MSPM0G1X0X_G3X0X_DFP.1.3.1`

执行 `tools\install-g3507-debug-tools.ps1` 下载 pyOCD 使用的 Pack，然后使用
Keil Pack Installer 安装同一个 Pack。在 Texas Instruments 设备系列中选择
`MSPM0G3507`，并确认工程使用以下配置：

- 主 Flash 算法：`MSPM0G1X0X_G3X0X_MAIN_128KB.FLM`
- SVD 文件：`MSPM0G350X.svd`

## 构建

在仓库根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-keil-mspm0g3507-app.ps1
```

脚本使用 `--compiler keil` 生成 SysConfig 文件，调用 Keil UV4 执行批量构建，
并检查受保护的 CCS 和 VS Code 文件未被修改。该过程不会擦除或写入目标板。
调试 AXF 输出到
`keil\mspm0g3507_app\Objects\mspm0g3507_app.axf`，HEX 文件输出到
`Objects\`，链接器 MAP 文件输出到
`keil\mspm0g3507_app\mspm0g3507_app.map`。

工程使用 SDK 中的 GCC Cortex-M0 FreeRTOS 移植层，以及本地目录中的
`freertos_port\ARM_CM0`。为了匹配 SDK Keil DriverLib 静态库，Arm Compiler 6
必须启用 short enum 和 short wchar ABI 设置：
`vShortEn=1`、`vShortWch=1`。

两种 VOFA 遥测模式默认关闭：
`VOFA_SPEED_PID_TELEMETRY_ENABLE=0U` 和 `GRAY_VOFA_TELEMETRY_ENABLE=0U`。
构建灰度 JustFloat 配置时，执行：

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-keil-mspm0g3507-app.ps1 `
  -VofaSpeedPidTelemetryEnable 0 -GrayVofaTelemetryEnable 1
```

灰度模式以 115200 波特率每 100 ms 发送一帧 22 通道、92 字节的数据：

- 通道 0..7：`raw`
- 通道 8..15：`normalized`
- 通道 16：`digital`
- 通道 17：`black_mask`
- 通道 18：`black_count`
- 通道 19：`line_error`
- 通道 20：`line_strength`
- 通道 21：`sequence`

所有值均为小端序 `float32`，JustFloat 帧尾为 `00 00 80 7F`。速度 VOFA 和灰度
VOFA 模式不能同时启用。灰度 `line_error` 仅用于观察，不会驱动电机目标或 PWM。
构建成功不代表传感器已经完成实物验收。

## 从 VS Code 打开

运行 VS Code 任务 `MSPM0G3507 App: Open Keil Project`，生成并打开本地 Keil 工程：
`keil\mspm0g3507_app\mspm0g3507_app.local.uvprojx`。该任务只负责生成并打开工程，
不会生成 SysConfig 文件、构建、下载或写入 Flash。

启动脚本为 `tools\open-keil-mspm0g3507-app.ps1`。如果 Keil 不在标准安装目录中，
请设置 `KEIL_ROOT`，或传入 `-KeilRoot` 参数。

## 调试和下载

工程配置为 CMSIS-DAP/SWD，`DriverSelection=4096`。DAPLink 探针连接方式如下：

- SWDIO 连接 PA19
- SWCLK 连接 PA20
- VTref 连接 3.3 V
- GND 共地
- Probe UID：选择本机检测到的 CMSIS-DAP 探针

在 Keil 的目标设置中选择本机检测到的 CMSIS-DAP 探针，然后明确执行 Download
或 Start/Stop Debugging。除非已经在实际硬件上完成连接、Flash 编程、断点、单步
和寄存器检查，否则仓库构建检查不对这些硬件操作作出保证。

调试器可以查看 `g_encoder_samples[0]` 到 `g_encoder_samples[3]`。电机运行时不要
设置断点。

共享应用还提供 `g_grayscale_snapshot`，可通过 SWD 观察。灰度传感器接线为
AD0=PB13、AD1=PB1、AD2=PB23、OUT=PA27，EN 悬空，传感器使用独立稳定的 5 V
电源。Keil 会根据 CCS 工程维护的 SysConfig 源文件生成 ADC 和 GPIO 配置，
不要手动编辑 `Generated/`。灰度传感器的实物验收需要检查全部八个地址通道，
与 Keil 构建相互独立。

快照保留 `digital` 字段以兼容现有接口：`1=white`、`0=black`；同时增加
`black_mask`（`bit N=1` 表示第 N 个通道为黑色）、`black_count`、`line_strength`
和带符号的 `line_error`。模拟量线误差根据归一化值计算，目前仅用于观察，后续
电机控制改动前不会参与控制。驱动程序提供 `g_grayscale_adc_timeout_count`
用于 SWD 调试。下载新的 AXF 后，如果 `g_grayscale_snapshot.sequence` 持续变化，
说明采样正在进行；如果超时计数器持续变化，说明 ADC0 没有报告 MEM0 转换完成标志。

可选的 `board_imu_yaw.c` 航向估算器会编译进共享应用工程，并在现有 IMU 任务中
运行。当 `IMU_YAW_ENABLE` 从默认值 `0U` 改为 `1U` 后，该估算器才会启用。它使用
BMI160 Z 轴陀螺仪、启动时陀螺仪零偏校准，以及左右轮编码器差速计算，并使用
`main.c` 中的 `IMU_YAW_TRACK_WIDTH_MM`。实测左右轮中心距离为 130 mm，车辆坐标系
为 +X 向前、+Y 向左、+Z 向上。将 `IMU_TELEMETRY_ENABLE` 也设为 `1U`，即可通过
UART 观察航向字段。

输出为 16 字节 JustFloat 帧，通道依次为 `yaw_deg`、`yaw_rate_dps` 和
`gyro_bias_z_dps`。进行 UART 航向测试时，使用以下 Keil 构建参数：

```text
-VofaSpeedPidTelemetryEnable 0 -ImuTelemetryEnable 1 -ImuYawEnable 1
```
