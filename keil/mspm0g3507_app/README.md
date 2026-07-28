# MSPM0G3507 Keil MDK 应用工程

这是一个独立的 Keil MDK 工程，对应现有的 MSPM0G3507 FreeRTOS 应用。
应用源码共享自 `../../mspm0g3507_app`；原始的 `.project`、`.cproject` 和
SysConfig 源文件仍由 CCS 工程维护。

项目总览见 [`../../README.md`](../../README.md)，当前状态和硬件风险见
[`../../docs/AI_HANDOFF.md`](../../docs/AI_HANDOFF.md)，工具版本和依赖见
[`../../docs/DEPENDENCIES.md`](../../docs/DEPENDENCIES.md)。共享应用功能说明见
[`../../mspm0g3507_app/README.md`](../../mspm0g3507_app/README.md)。

## 前置条件

工具版本和兼容范围以 [`../../docs/DEPENDENCIES.md`](../../docs/DEPENDENCIES.md) 为准。
本工程还要求安装 G3507 对应的 CMSIS-Pack，并使用工程配置指定的设备、Flash 算法和
SVD 文件。

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

按键、速度、灰度、巡线和 IMU VOFA 遥测模式默认关闭。按键遥测可通过以下参数临时开启：

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-keil-mspm0g3507-app.ps1 `
  -ButtonVofaTelemetryEnable 1
```

`APP_BUTTON_VOFA_TELEMETRY_ENABLE`、`APP_VOFA_SPEED_PID_TELEMETRY_ENABLE` 和
`APP_GRAY_VOFA_TELEMETRY_ENABLE` 默认值均为 `0U`。
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

所有值均为小端序 `float32`，JustFloat 帧尾为 `00 00 80 7F`。按键遥测帧为 20 字节，
通道顺序为 `PA7、PB12、PA8、PA30`，按下为 `1.0f`，释放为 `0.0f`。所有 UART VOFA
模式不能同时启用。灰度 `line_error` 仅用于观察，不会驱动电机目标或 PWM。
构建成功不代表传感器已经完成实物验收。

赛道巡线遥测默认关闭，可使用以下参数临时开启；它要求保持 `APP_IMU_YAW_ENABLE=1U`，并与
其它 UART0 VOFA 模式互斥：

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-keil-mspm0g3507-app.ps1 `
  -CourseFollowingVofaTelemetryEnable 1 -CourseFollowingVofaTelemetryIntervalMs 10
```

该模式发送 14 通道 JustFloat 帧：yaw、yaw rate、gyro bias、赛道航向目标、无线区标志、
巡线误差、四轮目标速度和四轮反馈速度。赛道控制行为、SB/SC 映射与保守转弯参数见共享应用 README。

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

共享应用工程现在编译 `algorithms/imu_fusion/imu_fusion.c` 纯六轴姿态融合器，并在现有 IMU 任务中
以 5 ms / 200 Hz 运行。`APP_IMU_YAW_ENABLE` 默认值为 `1U`，需要无 IMU 硬件的调试构建时可显式改为
`0U`。融合器使用三轴陀螺仪和三轴加速度计进行四元数姿态更新，启动时进行约一秒三轴零偏校准，
不读取左右轮编码器，也不需要轮距参数。车辆坐标系为 +X 向前、+Y 向左、+Z 向上。将
`APP_IMU_TELEMETRY_ENABLE` 默认值为 `0U`。需要通过 UART 观察融合调试字段时，显式设为 `1U`，即可开启
VOFA 输出。

输出为 56 字节 JustFloat 融合调试帧，共 13 个通道：
`yaw_deg`、`yaw_rate_dps`、`roll_deg`、`pitch_deg`、`accel_norm_g`、`acceleration_valid`、
`gyro_bias_x_dps`、`gyro_bias_y_dps`、`gyro_bias_z_dps`、`stationary_confirmed`、`calibrated`、
`valid`、`dt_s`。同时可通过 SWD 观察 `g_imu_fusion_snapshot`；通道 10/11 分别表示校准完成和当前
yaw 是否可用于控制。进行 UART 融合调试时，使用以下 Keil 构建参数：

硬件诊断不再占用 VOFA 通道。通过 SWD 观察 `g_imu_debug`，并确认 `chip_id=0xD1`、
`sample_successes` 持续增加、水平静止时 `last_sample.accel_z` 约为 `8192`；
`g_bmi160_diagnostics` 可进一步查看配置寄存器回读值。

发生 HardFault 时，可通过 SWD 读取 `g_hardfault_snapshot`。`active=1` 表示已捕获，
`stacked_pc`/`stacked_lr`/`stacked_sp` 是异常栈帧，`cfsr`/`hfsr`/`dfsr`/`mmfar`/`bfar`/`icsr` 是 SCB
状态镜像。Keil 启动 MSP 系统栈已扩大到 0x400 字节，以降低嵌套中断和 FreeRTOS 异常路径的栈帧破坏风险。

六轴 IMU 没有磁力计或其他外部航向来源，只能输出相对于启动方向的 yaw；加速度计用于倾斜校正，
不能消除长期 yaw 漂移。启动和重新标定期间车辆必须保持静止，未完成的倾斜、漂移和转向符号
仍需实车验收。

```text
-VofaSpeedPidTelemetryEnable 0 -ImuTelemetryEnable 1 -ImuYawEnable 1
```

共享应用默认启用 `rtos_monitor`，通过 SWD 观察 `g_rtos_monitor_snapshot`，不占用 UART0。
如需构建精简版本，可使用 `-RtosMonitorEnable 0` 临时关闭；该参数不会修改共享源码配置。
