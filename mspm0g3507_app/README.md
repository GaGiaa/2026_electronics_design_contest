# MSPM0G3507 WS2812 应用工程

本文档说明 G3507 应用工程的源码分层、构建方式、硬件连接、功能开关和调试方法。项目
总览和 AI 接手顺序见根目录 [`README.md`](../README.md)。
仓库级依赖、环境配置和 AI 交接规则分别见：

- [`docs/AI_HANDOFF.md`](../docs/AI_HANDOFF.md)：AI 必读规则、当前工程状态、硬件事实和遗留风险；
- [`docs/DEPENDENCIES.md`](../docs/DEPENDENCIES.md)：软件版本、工具链、源码和硬件依赖；
- [`docs/SETUP.md`](../docs/SETUP.md)：跨电脑环境配置和构建操作；
- [`docs/CODING_STYLE.md`](../docs/CODING_STYLE.md)：C 代码注释和接口文档规范。

Keil MDK 的工程模板、构建产物和 Keil 调试说明见
[`keil/mspm0g3507_app/README.md`](../keil/mspm0g3507_app/README.md)。

## 分层源码布局

应用源码按便于后续 G3507 应用复用的方式组织：

```text
app/         应用启动、profile API、共享状态和 FreeRTOS 任务
drivers/     板级和外设驱动，不包含应用任务策略
algorithms/  PID、编码器解码与滤波、线跟踪、yaw 和电机控制算法
protocols/   CRSF 和 VOFA 帧处理与混控
services/    可复用的 FreeRTOS 服务，例如任务监控
config/      应用、编码器、CRSF、RTOS monitor 和 FreeRTOS 配置
platform/    G3507 中断分发
```

每个模块只在规范目录中保留一份实现，并在 CCS 和 Keil 工程中各加入一次。根目录旧头文件
继续作为兼容 include 入口，只包含规范头文件，不包含重复声明或实现。新代码应使用规范路径。

`main.c` 只负责 SysConfig 初始化、调用 `app_startup()`、启动 FreeRTOS 调度器和异常停机
循环。应用组合逻辑属于 `app/`。公开的任务注册接口为 `app_tasks_motor_start()`、
`app_tasks_sensor_start()`、`app_tasks_io_start()` 和 `app_tasks_telemetry_start()`。
`app/app_profile.h` 只提供 profile 类型和 API。应用编译期配置集中在
`config/app_config.h`，宏使用 `APP_` 前缀；编码器和 RTOS monitor 的参数分别位于
`config/encoder_config.h` 和 `config/rtos_monitor_config.h`。可通过 SWD 观察的状态名称保持不变。

这是一个独立的 MSPM0G3507 FreeRTOS 应用工程。UART0 使用 PA10/PA11，配置为 115200、
8-N-1。PB22 通过电平转换器驱动四颗 5 V WS2812，所有电源必须共地。SPI1 PICO 输出到
PB22，频率为 2.666667 MHz；PB9 是未使用的 SPI 时钟输出。WS2812 的每一位分别编码为
`100`（0）或 `110`（1），对应 0.375 us 和 0.75 us 的高电平时间。

使用 `tools/build-mspm0g3507-app.ps1` 构建，输出文件为
`mspm0g3507_app/Debug/mspm0g3507_app.out`。

## CCS 工程与可移植构建

`.project`、`.cproject` 和 `.ccsproject` 用于 CCS 源码浏览、SysConfig 编辑以及目标和
调试配置。FreeRTOS 内核源码安装在本机 MSPM0 SDK 中，不存储在本仓库内，因此导入的 CCS
工程并不描述一个完整的独立应用构建。全新的 workspace 中直接点击
`Project > Build Project` 可能出现找不到 `FreeRTOS.h` 或 `pid.h`；过期的 CCS workspace
还可能选择旧版本的 SDK 或 SysConfig。

可复现的应用构建应使用仓库构建脚本。脚本会加入 SDK 中的 FreeRTOS include 和 port 目录、
仓库内 `algorithms/pid` 的 include 目录，以及所需的 FreeRTOS 内核源文件：

```powershell
powershell -ExecutionPolicy Bypass -File tools\configure-toolchain.ps1 -PersistUserEnvironment
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-app.ps1
```

如果必须使用 CCS 图形界面构建，需要在本机工程中配置相同的 SDK FreeRTOS include/port
目录和 `algorithms/pid` include 目录，然后将 SDK 中的 `list.c`、`queue.c`、`tasks.c`、
`portable\TI_ARM_CLANG\ARM_CM0\port.c` 和 `portasm.c` 加入工程并链接对应对象。这属于
每台电脑自己的 CCS workspace 配置，不应替换为仓库中的绝对路径。

静态集成测试为 `tests/test_mspm0g3507_app.ps1`。WS2812 任务由
`APP_WS2812_ANIMATION_ENABLE` 和 `APP_WS2812_STATUS_INDICATOR_ENABLE` 控制，默认分别为
`0U` 和 `1U`，两个开关不能同时启用。启用旧动画时，每 500 ms 点亮一个像素，通道值为
16，依次在四个像素上循环显示红、绿、蓝和白色。

默认状态指示模式中，1 号灯（索引 0）在 CRSF 链路有效时常亮绿色，链路无效时以 500 ms
周期闪烁红色；4 号灯（索引 3）每 500 ms 按红、蓝、绿、白顺序换色并保持点亮，2、3 号灯
保持熄灭。链路状态沿用现有 100 ms CRSF 有效帧超时判定。硬件验收需要实际观察 LED 并持续
回显 UART 输入；构建或测试命令本身不会执行 Flash 操作。

应用 CPU 由板载 40 MHz HFXT 和 SYSPLL 运行在 80 MHz。四路 10 kHz、双输入 PWM 硬件通道
使用 PA12/PA13、PA28/PA31、PA29/PB27 和 PB4/PB5。逻辑轮位映射为：前左 PA29/PB27、
前右 PB4/PB5、后左 PA28/PA31、后右 PA12/PA13。后左电机的极性相反，因此逻辑轮位的
PWM 输入顺序被反向处理。`BOARD_MOTOR_DIRECTION_FORWARD` 对所有逻辑轮位都表示车辆前进。
`board_motor_set_signed_duty()` 将带符号的占空比转换为现有的正转/反转双 PWM 映射；正值
表示车辆前进，负值表示倒车。电机电源、电机驱动器和 MCU 必须共地。硬件验收应从单个轮子
的低占空比开始；任何 Flash 写入操作都必须事先获得明确授权。

每个轮位还有一个 AB 增量编码器。硬件校准发现，实际物理输入与原始 SysConfig 名称不一致。
当前 `mspm0g3507_app.syscfg` 中的逻辑映射为：前左 PA15/PB24、前右 PA17/PA22（方向反向）、
后左 PA14/PA9、后右 PA16/PB20（方向反向）。输入使用上拉；每个 A 相在双沿触发中断，
B 相用于判断方向。`board_encoder` 明确声明电机机械参数：电机轴编码器为 13 线，减速比为
20:1，因此每个轮位输出轴每转有 260 条机械线。

默认 `BOARD_ENCODER_DECODE_MODE_A_PHASE_DUAL_EDGE` 统计 A 相双沿，因此每个输出轴转一圈
得到 520 counts。将编译期 `BOARD_ENCODER_DECODE_MODE` 设置为
`BOARD_ENCODER_DECODE_MODE_AB_PHASE_QUADRATURE_X4` 后，会通过正交状态解码器统计 AB 两相
的双沿，此时每个输出轴转一圈得到 1040 counts。修改模式后需要重新构建和烧录，并在使用
速度环之前低速手动转动输出轴一圈进行确认。轮径为 48 mm。

电机任务每次 10 ms 的迭代依次采样带符号编码器增量、累积计数和计算得到的 mm/s 速度，
然后更新 PWM。每个轮位使用一个增量式 PID 速度控制器。`algorithms/pid/` 是从外部
MotorLib 中受控复制的、仅包含平台无关 PID 核心的目录；CAN 协议、STM32 HAL、DJI 和
RobStride 代码没有被引入。可通过 SWD 写入的 `volatile g_motor_speed_targets_mm_s[4]`
提供普通的四轮 mm/s 目标值，初始时所有目标均为 0。

当 `CRSF_REMOTE_CONTROL_ENABLE=0U` 时，`volatile g_motor_debug` 提供单轮 SWD 调试覆盖。设置 `enable`，选择 `wheel`，再选择
`MOTOR_CONTROL_DEBUG_MODE_STOP`、`MOTOR_CONTROL_DEBUG_MODE_PWM` 或
`MOTOR_CONTROL_DEBUG_MODE_SPEED`。PWM 模式使用带符号的 `target_duty_percent`；速度模式
使用 `target_speed_mm_per_s` 和可写的 `speed_pid_params`（`kp`、`ki`、`kd`、
`output_limit`、`deadband`）。默认情况下，速度调试使用所选轮位在
`g_default_speed_pid_params` 中的参数；将 `use_speed_pid_override` 设为 `true` 后，才会
明确使用 `speed_pid_params` 中的参数。输出被限制在带符号的 100% 范围内。调试启用后，
所有未选中的轮位都会停止。启用调试或修改轮位、模式时，控制器状态会复位，并在输出恢复
前让所有轮位保持一个 10 ms 控制周期的 0 输出。电机运动时不要设置断点。

为便于观察，10 ms 电机任务会写入 `volatile g_encoder_samples[BOARD_MOTOR_COUNT]` 快照，
可通过 SWD 观察，不需要增加断点。调试速度环时，将 `config/app_config.h` 中的编译期开关
`APP_VOFA_SPEED_PID_TELEMETRY_ENABLE` 从 `0U` 改为 `1U`，并在 VOFA+ 中选择 JustFloat。
低优先级遥测任务每 10 ms 发送一个固定 20 字节帧。四个 `float32` 通道跟随
`g_motor_debug.wheel`，顺序为 `target_speed_mm_per_s`、`instant_feedback_speed_mm_per_s`、
`feedback_speed_mm_per_s` 和 `output_duty_percent`；帧尾为 `00 00 80 7F`。

启用 VOFA 模式后，UART0 输出由该模式独占，不会创建 UART 回显任务。IMU yaw 遥测和速度环
VOFA 遥测在编译期互斥。启用 VOFA 模式时不要向 UART0 发送文本。帧仍然通过
`board_uart_write()` 放入队列，并由专用 UART TX 任务发送，因此电机任务和编码器 ISR 不会
阻塞在 UART 上。如果 `g_motor_debug.wheel` 无效，遥测任务会安全地发送前左轮状态。初次
验证时，不要设置断点；使用调试器手动转动一个轮子，确认计数符号和轮位隔离后，再以低占空比
驱动车体。

观察灰度传感器时，使用 `-VofaSpeedPidTelemetryEnable 0 -GrayVofaTelemetryEnable 1` 构建，
并在 115200 baud 下选择 JustFloat。灰度任务每 100 ms 发送一个 22 通道、92 字节的小端
`float32` 帧：

| 通道 | 内容 |
| --- | --- |
| 0..7 | `raw[0..7]` |
| 8..15 | `normalized[0..7]` |
| 16 | `digital`（`1=白色`，`0=黑色`） |
| 17 | `black_mask`（第 N 位对应 `channel[N]`，`1=黑色`） |
| 18 | `black_count` |
| 19 | `line_error` |
| 20 | `line_strength` |
| 21 | `sequence` |

`app_state.h` 公开了以下用于 SWD 观察的 `volatile` 全局变量：`g_encoder_sample_sequence`、`g_grayscale_publish_sequence`、
`g_drive_control_publish_sequence`，以及在 `APP_IMU_YAW_ENABLE=1U` 时存在的 `g_imu_yaw_snapshot` 和
`g_imu_yaw_publish_sequence`。这些序列变量用于快照一致性保护，只应观察，不应通过调试器写入。灰度驱动另提供
`volatile g_grayscale_debug` 镜像，用于观察每个通道的 `white`、`black`、`gray_white`、`gray_black` 标定值，
以及 `digital` 和 `sequence`。该镜像不会反向修改驱动内部的私有标定数组。

帧尾为 `00 00 80 7F`。速度和灰度 VOFA 遥测互斥，同时启用会触发编译期错误。在任一
VOFA 模式下，UART 回显和 BMI160 文本输出都会被抑制。灰度 VOFA 字段仅用于观察；高档循迹
使用同一个灰度快照，但不改变该 VOFA 帧的通道定义。灰度快照
额外提供 `adc_timeout_mask`，用于防止 ADC 超时被误判为黑线。初次验证时，让黑线经过传感器，
确认 `normalized`、`black_mask`、`line_error` 和 `sequence` 同步变化。

## 黑线循迹闭环

`algorithms/line_control/` 使用现有 `PID_Position` 实现灰度位置误差到左右差速速度的
外环，随后由现有四轮速度 PID 跟踪每个轮位的 mm/s 目标。位置式 PID 默认参数为
`kp=0.08f`、`ki=0`、`kd=0`，转向输出上限为 300 mm/s；可通过 `volatile`
`g_line_control_debug` 使用 SWD 覆盖 PID 参数、最大转向速度、轮速上限、转向符号、
黑度阈值和丢线时间。`g_drive_control_snapshot` 通过 `app_state` 提供模式、SB、灰度、
PID、四轮目标和反馈速度的序列保护快照，供 SWD 观察和后续 VOFA 扩展。

循迹有效判据要求灰度 `sequence` 非零、本次采样 `adc_timeout_mask` 为 0，并满足
`line_strength` 进入阈值 800；已有效后使用退出阈值 400。丢线时冻结 PID 并保持上次
转向输出最多 100 ms，超时后四轮目标清零并复位 PID。本轮不新增线控 VOFA 帧，已有灰度、
速度 PID 和 IMU JustFloat 格式保持不变。

PA2 通过 TIMG8 CCP1 驱动无源蜂鸣器。`config/app_config.h` 提供编译期宏
`APP_BUZZER_FEATURE_ENABLE`、`APP_BUZZER_FREQUENCY_HZ`、`APP_BUZZER_DUTY_PERCENT`、
`APP_BUZZER_ON_TIME_MS` 和 `APP_BUZZER_OFF_TIME_MS`。默认值依次为关闭、2000 Hz、50%、200 ms
开启和 1800 ms 关闭。启用后创建独立的静态 FreeRTOS 任务，重复开关周期；禁用时 PWM
比较值初始化为 0，不创建蜂鸣器任务。无源蜂鸣器驱动电路和 MCU 必须共地。

BMI160 使用独立的 SPI0 控制器。模块接线为 `SCK` 到 PB18、`SDI`/MOSI 到 PB17、
`SDO`/MISO 到 PB19、低有效 `CS` 到 PB0。PA21 预留给未来的数据就绪中断，第一版不使用。
SPI1 仍专用于 WS2812。BMI160 模块必须使用 3.3 V 并与 MCU 共地；除非模块原理图明确要求，
不要同时给 VIN 和 3V3 供电。在 SPI 模式下，SA0 不是地址配置项。

启动时，驱动会按照 Bosch 参考流程产生一次 CS 低到高的空 SPI 事务，以便上电后切换到 SPI
模式。随后 `board_bmi160.c/.h` 校验 `CHIP_ID`（`0xD1`），执行软复位，启动加速度计和
陀螺仪，并将采样配置为 100 Hz、±4g 和 ±500dps。驱动执行官方复位后的 SPI 通信测试，
每次寄存器写入后等待 1 ms，并在报告成功前检查错误、电源模式、ODR、带宽和量程寄存器。
静态 FreeRTOS 任务每 10 ms 读取 12 字节的加速度加陀螺仪寄存器块（`0x0C` 到 `0x17`，
陀螺仪在前）。SPI 控制器使用 Motorola mode 3，以匹配 Bosch 参考示例。启用 IMU yaw 遥测
时，使用现有的静态 UART 帧队列发送 JustFloat 帧。SPI 事务有有限超时；初始化失败后每秒
重试一次，连续三次读取失败会触发重新初始化。

该实现已经通过静态集成检查和 TI Clang 构建。硬件验证已确认 `CHIP_ID=0xD1`、静止时 Z
轴加速度约为 1g，以及静止陀螺仪输出接近 0。构建和测试命令不会执行 Flash 写入；这些
软件结果不替代尚未完成的完整硬件验收。

`config/app_config.h` 提供编译期开关 `APP_IMU_TELEMETRY_ENABLE`，默认值为 `0U`。当它与
`APP_IMU_YAW_ENABLE` 同时启用时，独立遥测任务按配置周期通过现有 UART 帧队列发送一个
16 字节 JustFloat 帧。三个 `float32` 通道为 `yaw_deg`、`yaw_rate_dps` 和 `gyro_bias_z_dps`，
之后是标准的 `00 00 80 7F` 帧尾。IMU 采样是否运行由 `APP_IMU_YAW_ENABLE` 单独决定。

PA7、PB12、PA8 和 PA30 是四个外部上拉、低有效按键输入。独立的静态 `button_task` 每
10 ms 扫描一次，并在连续两次采样一致后确认状态。稳定的按下和释放边沿按照引脚顺序，
通过串行 UART 帧队列发送，例如 `key,pa7=down\r\n` 和 `key,pa7=up\r\n`。SysConfig 关闭
内部电阻，也不为这些输入启用 GPIO 中断。按键任务使用 128 字的栈；TI Clang map 报告的
栈大小为 512 字节，任务控制块为 76 字节，按键驱动状态为 12 字节。`config/app_config.h`
提供编译期开关 `APP_BUTTON_FEATURE_ENABLE`，默认值为 `0U`。设为 `1U` 后启用按键初始化、
状态扫描、UART 报告和按键任务的静态 RAM，同时保持 SysConfig 引脚定义不变。

## 八路灰度传感器

Ganv 无 MCU 八路传感器使用 74HC4051 模拟多路复用器。将 AD0 连接 PB13、AD1 连接 PB1、
AD2 连接 PB23、OUT 连接 PA27。EN 悬空；传感器内部下拉会使其保持使能。传感器地线连接
MCU 地线，传感器使用稳定的独立 5 V 电源供电。

`board_grayscale.c/.h` 按地址顺序选择通道：通道 0 为 000，通道 7 为 111。每次地址切换
后等待约 1 us，然后对 12 位 ADC 进行 8 次单次转换并求平均。驱动提供原始数组、0..4095
归一化数组和 8 位滞回结果。`digital` 使用 `1` 表示白色，`0` 表示黑色。应用生成
`black_mask`，其中第 N 位对应通道 N，1 表示黑色，同时生成 `black_count`、`line_strength`
和带符号整数 `line_error`。

可选的一维车辆 yaw 估计器实现在 `board_imu_yaw.c/.h` 中。只有
`APP_IMU_YAW_ENABLE=1U` 时才会创建 10 ms 的 `imu_task`；该任务负责 BMI160 初始化、采样、
失败重试和 yaw 更新。它使用配置的 `+/-500 dps` 量程转换 BMI160 Z 轴陀螺仪数据，在启动时
通过 100 个静止样本估计陀螺仪零偏，并以 98% 陀螺仪和 2% 编码器的权重融合左右差速编码器
角速度。初始轮距由 `config/app_config.h` 中的 `APP_IMU_YAW_TRACK_WIDTH_MM` 配置；测得的
左右轮中心距离为 130 mm。`APP_IMU_YAW_ENABLE` 默认值为 `1U`，关闭时不创建 IMU 采样任务；如需使用不带 IMU
硬件的 SWD 或电机调试构建，可通过 `-ImuYawEnable 0` 显式关闭。

估计器假设车辆坐标系为 `+X` 向前、`+Y` 向左、`+Z` 向上，Z 轴正角速度表示左转。如果
安装的传感器 Z 轴方向相反，将 `BOARD_IMU_YAW_GYRO_Z_SIGN` 改为 `-1.0f`。

当 `APP_IMU_YAW_ENABLE` 和 `APP_IMU_TELEMETRY_ENABLE` 都为 `1U` 时，独立的
`imu_vofa_task` 按 `APP_IMU_VOFA_TELEMETRY_INTERVAL_MS` 周期读取 yaw 快照，并发送前文所述的
三通道 JustFloat yaw 帧。默认周期为 10 ms，独立任务使用
`APP_IMU_VOFA_TELEMETRY_TASK_STACK_DEPTH` 栈配置。yaw 任务通过 `app_state` 发布带有效标志的
快照；BMI160 初始化或采样无效时，VOFA 任务跳过发送，不发送过期帧。yaw 相对于启动时的朝向，
并归一化到 `[-180, 180)`；六轴 IMU 没有磁力计或其他外部航向来源时，无法提供绝对 yaw 参考。

`APP_VOFA_SPEED_PID_TELEMETRY_ENABLE` 默认值为 `0U`。进行 yaw UART 测试时，使用以下参数构建：

```text
-VofaSpeedPidTelemetryEnable 0 -ImuTelemetryEnable 1 -ImuYawEnable 1
```

VOFA 速度遥测和 IMU yaw 遥测不能同时启用。

`ImuYawEnable` 和 `ImuTelemetryEnable` 这两个 PowerShell 参数保持兼容；可选的
`ImuVofaTelemetryIntervalMs` 参数覆盖独立的 IMU VOFA 发送周期。关闭 yaw 时，
`app_profile_t.enable_imu` 为 `false`，且不会生成 BMI160 任务。

`app/app_startup.c` 使用实测的逐通道标定值：

```text
white = {2834, 3064, 2150, 1924, 3099, 3032, 3182, 2467}
black = { 353, 1075,  139,  189, 1027,  593, 2033,  110}
```

`line_error` 根据归一化模拟值计算黑度 `4095-normalized[i]`，使用权重
`{-3500,-2500,-1500,-500,500,1500,2500,3500}`。当黑度总和为 0 时保持之前的误差。
当前这些标定数据由灰度任务和高档循迹控制共同读取；灰度 VOFA 帧仍只用于观察。
`APP_GRAY_VOFA_TELEMETRY_ENABLE`
默认值为 `0U`，可以通过构建脚本设置为 1 以发送前文描述的二进制帧。即使遥测关闭，
仍可通过 SWD 观察 volatile 的 `g_grayscale_snapshot`。构建成功不代表灰度传感器实物验收
完成。

## FreeRTOS CPU 与任务监控

`rtos_monitor` 模块默认开启，不占用 UART0。需要精简资源或排查监控影响时，可向应用或 Keil
PowerShell 构建脚本传入 `-RtosMonitorEnable 0` 临时关闭；传入 `-RtosMonitorEnable 1` 可显式开启。
该任务每 1000 ms 更新一次
`g_rtos_monitor_snapshot`，供 SWD 观察总 CPU 利用率、空闲率、任务名称、状态、优先级、
运行时间占比、微秒级运行时间以及栈高水位标记。发布快照时 `sequence` 为奇数，更新完成
后为偶数。

手动配置时，编辑 `config/app_config.h`，`APP_RTOS_MONITOR_ENABLE` 默认设为 `1U`，需要关闭时改为 `0U`。
`config/app_config.h` 和 `config/FreeRTOSConfig.h` 都包含这个共享头文件，因此只有一个
源码级开关。构建参数适合自动化或临时构建，并会覆盖头文件默认值，但不会修改文件。

运行时计数器使用未连接引脚的 `TIMG12` 定时器。定时器由 BUSCLK 除以 8 后以 10 MHz 运行；
在不改变现有定时器分配的情况下，这是可用的最高稳定自由运行频率。快照中的 `timer_hz`
记录实际频率。运行时间包含被中断任务执行期间的时间，因此第一版不会单独报告 ISR 占比。
计数器为 32 位，监控窗口使用无符号差值处理每个采样窗口的回绕。但是，随附的 FreeRTOS
内核没有完整保护每个任务累计运行时间计数器的定时器回绕。在 10 MHz 时基下，计数器约
429 秒回绕一次；长时间连续运行后，任务级数值可能不准确。

## CRSF 遥控输入

CRSF 遥控链路使用 UART3，速率 420000 baud、8-N-1，PB3 为 RX，PB2 为 TX。将接收机
TX 输出连接到 PB3，并与 MCU 共地。固件只接收 CRSF 数据，不向接收机上传遥测或其他帧。
接收机输出必须是 3.3 V、非反相 UART TTL。

`CRSF_REMOTE_CONTROL_ENABLE` 默认值为 `1U`。使用
`tools/build-mspm0g3507-app.ps1 -CrsfRemoteControlEnable 0` 可以构建不创建 CRSF 接收任务的
SWD 调试版本。CH3（通道索引 2）控制前进和后退，CH1（通道索引 0）控制差速转向，SB/CH5
（通道数组索引 6）控制底盘模式。标准 CRSF 范围 172..1811 以 992 为中心映射，并使用 20%
死区。默认最大轮速目标为 800 mm/s，可通过 `CRSF_MAX_SPEED_MM_PER_S` 修改。

SB 低档（小于 700）使底盘空闲，中档（700 到 1299）使用 CH1/CH3 手动差速，高档（不小于
1300）使用 CH3 作为基础速度并进入黑线循迹。模式切换时先输出一个 10 ms 的四轮零目标，
连续 100 ms 没有收到有效的打包 RC 帧时也会清零。方向符号可以通过 `CRSF_FORWARD_SIGN`
和 `CRSF_TURN_SIGN` 调整。`CRSF_REMOTE_CONTROL_ENABLE=0U` 时保留 `g_motor_debug` 单轮
SWD 调试覆盖路径；UART0 仍可用于现有调试和 VOFA 输出。

通过 SWD 观察时，可以展开 volatile 的 `g_crsf_debug` 结构体。其字段包括
`channels.channels[0..15]`、`link_active`、`last_valid_time_ms`、`valid_frame_count`、
`crc_error_count`、`frame_error_count` 和 `rx_overflow_count`。其中
`channels.channels[2]` 是 CH3，`channels.channels[0]` 是 CH1，`channels.channels[6]` 是
CH5/SB。线控状态可通过 `g_drive_control_snapshot` 观察。

协议和混控主机测试位于 `tests/test_crsf.ps1`。硬件验收前必须先让车轮悬空，或断开电机电源。
