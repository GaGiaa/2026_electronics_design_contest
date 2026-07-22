# MSPM0 核心板工程交接索引

最后更新：2026-07-22

## 使用规则

任何 AI 在查看、修改、构建、调试或烧录本仓库前，必须先完整阅读本文档。任务完成后必须
更新实际修改、验证结果、硬件事实和遗留风险。正文使用中文；代码标识符、命令、路径和
协议名称保持原样。不得混用 L1306 与 G3507 的 SysConfig、ELF、CMSIS-Pack 或引脚配置。

除非用户明确授权，不得执行 Flash 擦除、烧录、探针枚举、GDB 服务或会暂停运行中电机的
调试操作。

## 当前 Git 状态

当前分支为 `develop`，当前合并结果为 `7d98698`。三个分支按以下顺序使用普通非快进
合并完成：

- 基线 `d2a97dc`：关闭 VOFA 速度遥测默认开关；
- `af02c42`：合并 `develop_1`，加入 IMU Yaw 算法和 JustFloat 遥测；
- `8181750`：合并 `develop_2`，加入灰度线跟踪和灰度 VOFA 遥测；
- `7d98698`：合并 `develop_3`，加入编码器速度滤波、四轮速度 PID 和四通道速度遥测。

本文档整理与功能合并分开保存，便于后续追踪和交接。

## 工程与工具链

| 核心板 | CCS 工程 | 芯片与封装 | LED | UART0 |
| --- | --- | --- | --- | --- |
| L1306 | `mspm0l1306_bringup/` | MSPM0L1306，VQFN-32 | PA3 | PA8 TX，PA9 RX |
| G3507 | `mspm0g3507_bringup/` | MSPM0G3507，LQFP-64 | PB22 | PA10 TX，PA11 RX |

G3507 应用工程为 `mspm0g3507_app/`，Keil MDK 工程为 `keil/mspm0g3507_app/`；
FreeRTOS 基线工程为 `mspm0g3507_freertos/`。主要工具如下：

- CCS：`D:\Software\ti\ccs2020`
- MSPM0 SDK：`D:\Software\ti\ccs2020\mspm0_sdk_2_11_00_07`
- SysConfig：`D:\Software\ti\ccs2020\sysconfig_1.26.2`
- TI Clang：`D:\Software\ti\ccs2020\ccs\tools\compiler\ti-cgt-armllvm_4.0.3.LTS\bin\tiarmclang.exe`
- Keil：`D:\Keil_v5`；调试器：Horco CMSIS-DAP v2
- pyOCD：`tools/.venv`，版本约束 `>=0.45,<0.46`

## 构建与安全边界

~~~powershell
& 'D:\Software\ti\ccs2020\ccs\utils\bin\gmake.exe' -C mspm0l1306_bringup\Debug all
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-app.ps1
powershell -ExecutionPolicy Bypass -File tools\build-keil-mspm0g3507-app.ps1
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-freertos.ps1
~~~

G3507 CCS ELF 为 `mspm0g3507_app/Debug/mspm0g3507_app.out`；Keil 输出为
`keil/mspm0g3507_app/Objects/mspm0g3507_app.axf`、`.hex` 和
`keil/mspm0g3507_app/mspm0g3507_app.map`。

构建脚本支持临时参数 `-VofaSpeedPidTelemetryEnable`、
`-GrayVofaTelemetryEnable`、`-ImuTelemetryEnable`、`-ImuYawEnable` 和
`-EncoderDecodeMode`。未显式传入时不覆盖源码默认值。

`tools/flash-and-debug-g3507.ps1` 的 `Load` 会执行 `--erase chip` 并写入 Flash；
执行前必须取得用户本次操作的明确授权。构建、静态检查和 `GdbServer` 不会写入 Flash。

## G3507 应用硬件

### 电机、编码器与 PID

CPU 为 80 MHz，四路 PWM 为 10 kHz。逻辑轮位引脚如下：

| 轮位 | PWM | 编码器 A/B |
| --- | --- | --- |
| 前左 | PA29/PB27 | PA15/PB24 |
| 前右 | PB4/PB5 | PA17/PA22 |
| 后左 | PA28/PA31 | PA14/PA9 |
| 后右 | PA12/PA13 | PA16/PB20 |

编码器电机轴为 13 线、减速比 20:1，输出轴机械线数 260。默认 A 相双沿模式为每转
520 counts；AB 正交 X4 模式为每转 1040 counts。GPIOA/GPIOB 共用 GROUP1 IRQ。
更换电机、接线或解码模式后，必须低速手动转轴确认方向、单圈计数和四轮隔离。

电机任务每 10 ms 按“编码器采样、速度控制、带符号 PWM 输出”的顺序运行。
`g_motor_speed_targets_mm_s[4]` 默认全为 0；`g_motor_debug` 支持单轮 STOP、
PWM 和 SPEED 调试。调试轮位或模式切换时，PID 归零并保持一个周期的 0% 输出。
速度滤波使用 5 点滑动计数窗口；`instant_speed_mm_per_s` 是当前周期值，
`speed_mm_per_s` 是 PID 使用的滤波值。PID 支持积分限幅、积分分离、抗饱和、微分滤波
和输出变化率限制，默认 `ki=0`、`kd=0`。

### BMI160、IMU Yaw 与其他外设

BMI160 使用 SPI0：`SCK=PB18`、`MOSI=PB17`、`MISO=PB19`、`CS=PB0`，
配置为 100 Hz、±4g、±500 dps。`board_imu_yaw.c/.h` 在现有 10 ms `imu_task`
中运行，不创建新任务；使用 100 个静止样本校准陀螺仪 Z 轴零偏，并以 130 mm 轮距融合
左右轮差速角速度。输出 yaw 归一化到 `[-180, 180)`。如安装方向相反，优先修改
`BOARD_IMU_YAW_GYRO_Z_SIGN`；六轴 IMU 不提供绝对航向。

四个低有效按键为 PA7、PB12、PA8、PA30，默认 `BUTTON_FEATURE_ENABLE=0U`。
八路灰度传感器使用 74HC4051：`AD0=PB13`、`AD1=PB1`、`AD2=PB23`、
`OUT=PA27`；每路 ADC 采样 8 次取平均。`digital` 为 `1=白色`、`0=黑线`；
线跟踪发布 `black_mask`、`black_count`、`line_strength` 和 `line_error`，
目前只用于观察，不改变电机 PWM。

当前灰度标定值：

~~~text
white = {2834, 3064, 2150, 1924, 3099, 3032, 3182, 2467}
black = { 353, 1075,  139,  189, 1027,  593, 2033,  110}
~~~

WS2812 使用 SPI1 输出到 PB22，默认每 500 ms 点亮一颗灯；蜂鸣器默认关闭。

## UART 遥测

UART0 同一时间只能运行一种遥测模式；速度 VOFA、灰度 VOFA、IMU Yaw 之间有编译期互斥
保护，任一遥测模式启用时 UART 回显任务不会创建。所有帧均为小端 IEEE-754 `float32`，
帧尾为 `00 00 80 7F`。

| 模式 | 开关 | 周期 | 帧内容 |
| --- | --- | --- | --- |
| 速度环 | `VOFA_SPEED_PID_TELEMETRY_ENABLE` 默认 `0U` | 10 ms | 4 通道、20 字节 |
| 灰度线跟踪 | `GRAY_VOFA_TELEMETRY_ENABLE` 默认 `0U` | 100 ms | 22 通道、92 字节 |
| IMU Yaw | `IMU_TELEMETRY_ENABLE=1U` 且 `IMU_YAW_ENABLE=1U` | 10 ms | 3 通道、16 字节 |

速度通道依次为 `target_speed_mm_per_s`、`instant_feedback_speed_mm_per_s`、
`feedback_speed_mm_per_s`、`output_duty_percent`。灰度通道依次为
`raw[0..7]`、`normalized[0..7]`、`digital`、`black_mask`、`black_count`、
`line_error`、`line_strength`、`sequence`。IMU 通道依次为 `yaw_deg`、
`yaw_rate_dps`、`gyro_bias_z_dps`。

## 公开接口与验证

公开接口包括：

- `board_imu_yaw_state_t`、`board_imu_yaw_init()`、`board_imu_yaw_update()`；
- `line_tracking_state_t`、`line_tracking_result_t`、线跟踪初始化/更新函数；
- `encoder_speed_filter_t`、编码器滤波初始化/更新函数；
- 扩展后的 `board_grayscale_snapshot_t`、`motor_control_wheel_status_t`；
- 通用 `vofa_justfloat_encode()`、`vofa_justfloat_encode3()`、
  `vofa_justfloat_encode4()`。

主机测试和静态集成检查：

~~~powershell
powershell -ExecutionPolicy Bypass -File tests\test_board_imu_yaw.ps1
powershell -ExecutionPolicy Bypass -File tests\test_line_tracking.ps1
powershell -ExecutionPolicy Bypass -File tests\test_vofa_justfloat.ps1
powershell -ExecutionPolicy Bypass -File tests\test_motor_control.ps1
powershell -ExecutionPolicy Bypass -File tests\test_motor_control.ps1 -EncoderDecodeMode 1
powershell -ExecutionPolicy Bypass -File tests\test_motor_control.ps1 -EncoderDecodeMode 2
powershell -ExecutionPolicy Bypass -File tests\test_mspm0g3507_app.ps1
powershell -ExecutionPolicy Bypass -File tests\test_keil_mspm0g3507_app.ps1
~~~

Current CRSF transmitter mapping: CH3 (index 2) is forward/reverse from the left stick vertical axis; CH1 (index 0) is differential steering from the right stick horizontal axis.
This mapping supersedes the earlier CH2/CH4 mapping described in older notes.

构建通过只证明 SysConfig、编译、链接和静态集成检查通过，不代表硬件验收完成。

## 待完成硬件验收

- BMI160 长期零偏、静止漂移、左右转 yaw 符号和急转弯响应；
- 编码器四轮方向、单圈计数、解码模式和低速隔离；
- 四轮速度 PID 低速调参、滤波延迟和 PWM 抖动；
- 灰度白黑归一化、通道位图、横向误差、全白、全黑和丢线状态；
- VOFA+ 曲线数量、通道顺序、帧尾和 UART 实际接收；
- WS2812、蜂鸣器和按键实物响应。

未完成上述项目时，不得仅凭构建结果宣称实体功能验收完成。

## CRSF 遥控输入

G3507 应用新增可选 CRSF 接收链路，使用 UART3：PB2 为 TX，PB3 为 RX，
420000 baud、8-N-1、RX FIFO 中断。接收头 TX 连接 PB3，双方共地；接收头
必须输出 3.3 V、非反相 UART TTL。此功能只接收 `RC_CHANNELS_PACKED`，不向
接收头上传任何数据。UART0 仍保持 PA10/PA11，用于原有调试和 VOFA。

编译开关为 `CRSF_REMOTE_CONTROL_ENABLE`，构建脚本参数为
`-CrsfRemoteControlEnable 0|1`，默认配置由 `crsf_config.h` 决定。当前实测遥控器
使用 CH3（索引 2）控制前后，CH1（索引 0）控制差速转向；这会覆盖旧记录中的
CH2/CH4 映射。通道范围为 172..1811，中位 992，死区 5%，默认最大目标速度为
300 mm/s，可通过 `CRSF_MAX_SPEED_MM_PER_S` 修改。
左右混控统一归一化；连续 100 ms 没有有效帧时四轮目标清零。开启 CRSF
构建时，SWD 单轮调试覆盖路径被编译排除；默认构建保留原行为。

CRSF 原始通道、有效帧计数、CRC/帧错误计数、最后有效帧时间、链路状态和
UART 接收溢出计数可通过 SWD 观察。主机测试为
`powershell -ExecutionPolicy Bypass -File tests\test_crsf.ps1`。

## Git 操作授权规则

除非用户明确要求，AI 不得自行执行 `git commit`、`git push`、创建 Pull Request 或
向远程仓库发布内容。普通开发任务只允许修改工作区并运行必要的本地验证。
