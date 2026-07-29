# MSPM0 核心板工程交接索引

## H723 JY901S UART9 IMU

- STM32CubeMX 已配置并生成 UART9：`PG0=UART9_RX`、`PG1=UART9_TX`、234000 bit/s、DMA1 Stream2 RX 与 DMA/UART9 中断。
- `App/app_jy901s` 解析标准 `0x55 0x51/0x52/0x53` 帧及温度；`jy901sTask` 用 FreeRTOS 5 ms 时基通过 ReceiveToIdle DMA 环形缓冲发布数据。
- `g_h723_debug` 是 H723 唯一的 Keil Watch 调试入口，按 `system`、`uart8`、`crsf`、`chassis`、`fdcan`、`m2006[3]` 和 `jy901s` 分组。它只用于观察，不得作为业务输入；任务与中断可独立更新字段，跨字段组合不保证原子一致。`APP_JY901S_VOFA_TELEMETRY_ENABLE=0U` 默认关闭，开启后 UART8 发送 `Ax, Ay, Az, Gx, Gy, Gz, Roll, Pitch, Yaw, TemperatureC` 十通道 JustFloat；它与健康遥测编译期互斥。
- 未执行烧录、探针/SWD/GDB、JY901S 或 VOFA 实物测试。详细接线、单位和验收条件见 [`docs/STM32H723_JY901S.md`](STM32H723_JY901S.md)。

最后更新：2026-07-28

## 使用规则

任何 AI 在查看、修改、构建、调试或烧录本仓库前，必须完整阅读本文档和根目录
`README.md`。任务完成后必须更新实际改动、验证结果、硬件事实和遗留风险。说明性正文
使用中文；代码标识符、命令、路径、协议名称、芯片型号和工具名称保持原样。

除非用户明确授权，不得执行 Flash 擦除、烧录、探针枚举、GDB 服务或会暂停运行中电机的
调试操作。不得混用 L1306 与 G3507 的 SysConfig、设备包、CMSIS-Pack、linker 或引脚配置。

## 文档职责与 AI 接手顺序

项目总览、工程地图和任务结束后的更新决策表见根目录 `README.md`。其他文档职责如下：

- `docs/AI_HANDOFF.md`：当前状态、硬件事实、验证结果、遗留风险和 AI 操作规则；
- `docs/DEPENDENCIES.md`：软件版本、工具链、源码依赖和硬件依赖的唯一权威来源；
- `docs/SETUP.md`：跨电脑环境配置、工具安装、构建和调试操作；
- `docs/CODING_STYLE.md`：C 注释、接口、算法、驱动、FreeRTOS 任务和文档写作规范；
- 各工程 `README.md`：对应工程的功能、接线、构建输出和工程级调试说明。

新 AI 必须按以下顺序阅读：

1. 根目录 `README.md`；
2. 本文档；
3. `docs/DEPENDENCIES.md`；
4. `docs/SETUP.md`；
5. `docs/CODING_STYLE.md`；
6. 与任务对应的工程 README；
7. 源码、工程文件和测试脚本。

涉及 Keil 构建时，还必须读取 `keil/mspm0g3507_app/README.md`。

## 当前 Git 状态

当前工作区分支为 `develop`，远端跟踪分支为 `origin/develop`。本次巡线默认参数修订基于
提交 `12c4258`，实际提交状态、HEAD 和远端领先关系仍应以执行时的 `git status` 与
`git log` 输出为准。

此前的功能合并基线按以下顺序完成：

- `d2a97dc`：关闭 VOFA 速度遥测默认开关；
- `af02c42`：加入 IMU Yaw 算法和 JustFloat 遥测；
- `8181750`：加入灰度线跟踪和灰度 VOFA 遥测；
- `7d98698`：加入编码器速度滤波、四轮速度 PID 和四通道速度遥测；
- `4d0277a`：完成 G3507 应用工程分层迁移；
- `24b4073`：整理工程文档体系并完成中文化。

提交号只能记录已经通过 `git log` 验证的历史事实，不得预先填写尚未创建的提交号。

## 工程与依赖索引

| 工程 | 芯片与用途 | 主要入口 |
| --- | --- | --- |
| `mspm0l1306_bringup/` | MSPM0L1306 初始调试 | `tools/build-mspm0l1306.ps1` |
| `mspm0g3507_bringup/` | MSPM0G3507 初始调试 | `tools/build-mspm0g3507.ps1` |
| `mspm0g3507_freertos/` | G3507 FreeRTOS 静态分配基线 | `tools/build-mspm0g3507-freertos.ps1` |
| `mspm0g3507_app/` | G3507 分层应用 | `tools/build-mspm0g3507-app.ps1` |
| `keil/mspm0g3507_app/` | G3507 应用 Keil MDK 工程 | `tools/build-keil-mspm0g3507-app.ps1` |

工具版本和完整源码依赖见 `docs/DEPENDENCIES.md`。跨电脑安装和构建步骤见
`docs/SETUP.md`。G3507 应用的功能、接线、遥测和接口说明见
`mspm0g3507_app/README.md`。

## 构建与安全边界

常用构建命令如下：

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-mspm0l1306.ps1
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507.ps1
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-freertos.ps1
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-app.ps1
powershell -ExecutionPolicy Bypass -File tools\build-keil-mspm0g3507-app.ps1
```

`app_state.h` 公开 `g_encoder_sample_sequence`、`g_grayscale_publish_sequence` 和
`g_drive_control_publish_sequence` 供 SWD 观察；启用 `APP_IMU_YAW_ENABLE=1U` 时还公开
`g_imu_fusion_snapshot` 和 `g_imu_fusion_publish_sequence`。融合快照包含 yaw、roll/pitch、加速度可信度、
三轴 gyro bias、校准状态、实际 `dt_s` 和 `valid`。这些 `volatile` 变量是快照序列保护状态，
只应观察，不应通过调试器写入。灰度驱动的 `volatile g_grayscale_debug` 是标定参数和数字状态的只读镜像，

IMU 硬件诊断通过 `volatile g_imu_debug` 提供：它包含 BMI160 芯片 ID、初始化/采样状态码、初始化尝试数、
采样成功数、连续失败数、最近一次原始六轴样本和配置寄存器回读值；驱动层 `g_bmi160_diagnostics` 也可直接
通过 SWD 观察。IMU VOFA 调试帧包含前 12 个融合通道，最后一个通道为 `dt_s`；上述硬件诊断量不占用 VOFA 通道，仍可通过 SWD 观察。
`chip_id` 应为 `0xD1`，
水平静止时 `last_sample.accel_z` 应约为 `8192`，`sample_successes` 应持续增长。
不改变驱动内部仍保持 `static` 的标定数组。任务栈、通信缓存、编码器滤波器和 PID 内部状态继续保持私有链接。

G3507 CCS 应用输出为 `mspm0g3507_app/Debug/mspm0g3507_app.out`。Keil 输出为
`keil/mspm0g3507_app/Objects/mspm0g3507_app.axf`、对应 HEX 和
`keil/mspm0g3507_app/mspm0g3507_app.map`。

`mspm0g3507_app` 的 CCS 工程用于源码浏览、SysConfig 和调试目标配置；由于 FreeRTOS
内核源码来自本机 SDK，跨电脑可复现构建必须使用 PowerShell 构建脚本。全新 CCS
workspace 直接执行 `Project -> Build Project` 不是该应用的完整构建入口。

构建、SysConfig 生成、主机测试、静态检查和 GDB server 不会写入 Flash。任何会执行
`--erase chip`、下载 firmware 或连接目标板的操作，都必须获得用户本次操作的明确授权。

## 当前硬件事实

### G3507 电机与编码器

- CPU 为 80 MHz，四路 PWM 为 10 kHz；详细轮位引脚见 `mspm0g3507_app/README.md`；
- 电机轴编码器为 13 线，减速比 20:1，输出轴机械线数为 260；
- 默认 A 相双沿模式为每圈 520 counts，AB 正交 X4 模式为每圈 1040 counts；
- 更换电机、接线或解码模式后，必须低速手动确认方向、单圈计数和四轮隔离；
- 电机运动时不得设置断点，初次调试应让车轮悬空或断开电机电源。

### BMI160 与 IMU Yaw

BMI160 使用 SPI0：`SCK=PB18`、`MOSI=PB17`、`MISO=PB19`、`CS=PB0`，配置为
200 Hz、±4g、±500 dps。驱动已确认 `CHIP_ID=0xD1`，静止时 Z 轴加速度约为 1g，
BMI160 软复位后需要重新执行 SPI 接口选择事务，再进行配置寄存器写入和回读；
静止陀螺仪输出接近 0。纯六轴 IMU 融合使用三轴陀螺仪和三轴加速度计，不读取编码器辅助 yaw，
只能提供相对于启动方向的航向。加速度只有模长位于 `0.75~1.25 g` 且相对预测重力的方向误差
不超过 `35 deg` 时才参与重力校正；连续方向失配 `3 s` 后进入恢复。长期零偏、漂移、方向符号、
阈值/恢复时间的实车适配、倾斜补偿和急转弯响应仍属于待完成硬件验收。

### 灰度、WS2812、按键和 OLED

八路灰度传感器使用 74HC4051：`AD0=PB13`、`AD1=PB1`、`AD2=PB23`、`OUT=PA27`。
灰度结果通过 `g_grayscale_snapshot` 提供 SWD 和 VOFA 观察，并由高档循迹模式读取其
`line_error`、`line_strength`、`sequence` 和 `adc_timeout_mask`。WS2812 使用 SPI1 输出到
PB22；四针 SSD1306 OLED 使用 I2C0：`SDA=PA0`、`SCL=PA1`，默认地址为 `0x3C`。
当前 `app/app_startup.c` 使用的八路逐通道标定默认值为 `white={1239,2393,803,709,2596,2254,3040,829}`、
`black={72,64,67,81,97,76,410,76}`；数组下标按 `74HC4051` 地址顺序对应通道 0..7。标定值已同步到
工程 README 和应用静态集成测试，但白黑基准、归一化、黑线位图和循迹抗干扰能力仍需结合灰度 VOFA 与实物验证。
当前 `test_board_grayscale.ps1` 的 GPIO mock 未记录 `AD0/AD1/AD2` 输出，尚不能自动验证 `000..111`
地址序列及左右物理位置映射；这属于测试覆盖缺口，后续应通过扩展 mock 或灰度 VOFA 实物观察补充确认。
WS2812、蜂鸣器、按键和 OLED 的实物响应仍需单独验收。

### HC-SR04 超声波测距

当前采用方案 A：`TRIG=PB10`，`ECHO=PB11`，`VCC=5 V`，`GND=MCU GND`。HC-SR04 的
`ECHO` 通常为 5 V，必须经过分压或电平转换后再进入 `PB11`；推荐
`ECHO -> 10 kOhm -> PB11 -> 18 kOhm -> GND`，不能直连。PB10/PB11 原本未占用，且不会
改变 `PB8` 舵机 PWM、`PB9` WS2812 SPI 时钟、TIMG0/TIMG6/TIMG8 的既有功能。

方案 A 使用 PB11 GPIO 双边沿中断记录 Echo 高电平宽度，复用 `TIMG12` 的 10 MHz 运行时
计时器，不新增定时器。驱动在每次触发前清理 pending 状态，成功或超时后关闭 Echo 中断，
避免迟到边沿污染下一次测量。10 MHz 计时器的 0.1 us 量化只对应约 0.017 mm；ISR 两个
边沿的公共进入延时通常抵消，但 1 us 的边沿延时差对应约 0.17 mm，5 us 对应约 0.86 mm。
由于当前没有示波器实测数据，不能把上述换算当作最终误差保证；中断屏蔽、临界区和高优先级
中断造成的实际边沿差仍需硬件验收。

功能默认由 `APP_HCSR04_ENABLE=0U` 关闭，HC-SR04 JustFloat 遥测由
`APP_HCSR04_TELEMETRY_ENABLE=0U` 关闭。启用后，`g_hcsr04_snapshot` 发布
`distance_mm`、`echo_time_us`、`valid`、`timeout_count` 和 `sequence`；3 通道 UART0
JustFloat 顺序为距离、Echo 时间和有效标志，并与其他 UART0 VOFA 遥测互斥。

### UART 遥测与 CRSF

UART0 同一时间只能运行一种遥测模式。按键 VOFA、速度 VOFA、灰度 VOFA、通用巡线 VOFA、赛道巡线 VOFA 和 IMU Yaw 在编译期互斥，
启用遥测时不创建 UART 回显任务。当前 CRSF 映射为 CH3（索引 2）控制前进和后退，
CH1（索引 0）控制手动模式差速转向，SB/CH5 使用通道数组索引 6，SC 使用通道数组索引 7；
SB 或 SC 任一低档为空闲，SB/SC 中/中为手动、中/高为 yaw 锁定、高/中为通用巡线、高/高为赛道巡线；连续
100 ms 没有有效帧时四轮目标清零。`CRSF_REMOTE_CONTROL_ENABLE` 默认值为 `1U`，
仍可通过构建参数设为 `0` 以保留无 CRSF 的 SWD 单轮电机调试路径。

详细帧格式、构建参数和调试变量见 `mspm0g3507_app/README.md`。CRSF 硬件验收前必须
让车轮悬空或断开电机电源。

### 黑线循迹闭环

新增 `algorithms/line_control/`，使用现有 `PID_Position` 将灰度位置误差转换为左右
差速速度，再由四轮速度 PID 生成 PWM。当前默认位置式 PID 为 `kp=0.4f`、`ki=0`、`kd=0`，
默认转向符号为 `-1.0f`，转向输出上限为 300 mm/s。`g_line_control_debug` 提供 SWD 可写的 PID、轮速、转向符号、
灰度阈值和丢线时间参数；新增 `algorithms/yaw_control/` 及可写的
`g_yaw_control_debug`，用于设置目标 yaw、yaw 位置 PID、转向限幅和符号。yaw 位置环默认
`kp=15.0f`、`ki=0`、`kd=0`、死区 `0.1 deg`，转向上限 `700 mm/s`，轮速上限 `800 mm/s`，
默认 `turn_sign=-1.0f`；位置环按 50 ms 更新，10 ms 电机速度环执行四轮目标；`g_drive_control_snapshot` 通过 `app_state` 提供模式、SB/SC、
灰度、yaw 诊断、PID、四轮目标和反馈速度的序列保护快照。

巡线位置外环默认周期由 `APP_LINE_CONTROL_INTERVAL_MS` 配置，默认值为 50 ms；灰度采样任务和四轮速度
闭环仍为 10 ms。有效模拟 `line_error` 在 `line_control` 内使用默认 30 ms 时间常数的一阶低通，滤波器
在首次有效样本和丢线恢复时重新初始化。位置 PID 仍使用基础实现，`PID_POSITION_VARIANT_ADVANCED`
不启用，当前 `Ki/Kd` 保持为 0。位置环降频不影响 10 ms 的 ADC 超时、灰度有效性和丢线安全判定。

线控要求 `sequence` 非零、ADC 超时掩码为 0 且 `line_strength` 达到进入阈值 800；已有效
后使用退出阈值 400。丢线时冻结 PID 并保持上次转向输出 100 ms，之后清零目标并复位。
新增巡线 VOFA 遥测任务，默认由 `APP_LINE_CONTROL_VOFA_TELEMETRY_ENABLE=0U` 关闭，周期默认
为 20 ms，发送 18 通道、76 字节 JustFloat 帧。通道依次包含底盘模式、CRSF 链路、灰度误差、
黑度、有效标志、丢线时间、ADC 超时掩码、基础速度、转向输出、左右目标速度、左右平均反馈速度、
左右平均 PWM 以及位置 PID 的 P/I/D 输出。任务只读取 `g_drive_control_snapshot` 和电机状态快照，
通过现有 UART TX 队列发送，不直接访问巡线控制器内部状态。

### 本轮赛道巡线、IMU 漂移修正与遥测回迁

- 新增 `CRSF_DRIVE_MODE_COURSE_FOLLOWING`，SB/SC 真值表已固定为任一低档空闲、中/中手动、
  中/高 yaw 锁定、高/中通用巡线、高/高赛道巡线；低档优先于其它组合。
- 新增 `algorithms/course_following/`。赛道无线区需连续 5 个样本确认，航向保持目标为
  `0 deg`、`175 deg`，后续每半圈 `-5 deg`；全黑、IMU 无效与 `APP_IMU_YAW_ENABLE=0U` 均安全停车。
  短暂丢线不切换航向保持。边缘双黑直角转弯的保守默认值为直行 `150 mm/s`、外/内轮
  `+300/-200 mm/s`，最短 500 ms、中心线连续 3 样本退出、2000 ms 超时、100 ms 重触发抑制。
- 进入赛道模式时电机任务清零目标并请求 IMU yaw 重新标定；必须看到 yaw 快照失效后重新有效才允许赛道
  控制器输出。`app_drive_control_snapshot_t` 增加赛道航向目标和无线区/航向保持快照字段。
- `algorithms/imu_fusion/` 已替换旧 `board_imu_yaw`。融合器使用四元数 Mahony/互补校正，启动和
  请求重标定时约一秒估计三轴陀螺仪零偏；加速度模长异常时回退为纯陀螺仪积分，静止确认后缓慢
  跟踪零偏。IMU 任务和 BMI160 ODR 已改为 5 ms / 200 Hz，应用状态和 VOFA 已改为完整融合诊断接口。
- 新增 `tests/test_imu_fusion.ps1`，覆盖三轴校准、纯 IMU yaw、静态倾斜、加速度失效回退、实际
  `dt`、重标定和非法输入；旧 `test_board_imu_yaw.*` 已删除。
- 新增默认关闭的 `APP_COURSE_FOLLOWING_VOFA_TELEMETRY_ENABLE`。默认 10 ms 发送 14 通道
  JustFloat：yaw、yaw rate、gyro bias、赛道航向目标、无线区标志、循迹误差、四轮目标速度、四轮反馈速度。
  它要求 `APP_IMU_YAW_ENABLE=1U`，与其它全部 UART0 VOFA 遥测及 UART 回显互斥；CCS/Keil 构建参数为
  `CourseFollowingVofaTelemetryEnable` 与 `CourseFollowingVofaTelemetryIntervalMs`。
- 已新增赛道算法、14 通道编码、CRSF 真值表、IMU 漂移和配置互斥主机测试，并更新 CCS/Keil 源文件清单。
- 已执行全部 `tests\*.ps1`，并通过 `tools\build-mspm0g3507-app.ps1`、
  `tools\build-mspm0g3507-app.ps1 -CourseFollowingVofaTelemetryEnable 1`、
  `tools\build-keil-mspm0g3507-app.ps1` 与
  `tools\build-keil-mspm0g3507-app.ps1 -CourseFollowingVofaTelemetryEnable 1`。
  后续仍需在安全的实车条件下验证 IMU 漂移、直角转弯和赛道循迹响应；本轮未执行 Flash、烧录、探针、GDB/SWD
  或任何实车操作。

### 电机任务 HardFault 诊断与栈修复

- Keil 硬故障现场的异常堆栈表明，首次异常发生在 PendSV 的 `vTaskSwitchContext()`，而非
  `HardFault_Handler` 或外设中断。故障指令读取当前任务 TCB 的 `pxStack` 字段时，字段已经被改写为
  无效地址；现场 TCB 地址和栈指针均对应电机任务。
- `motor_task()` 因赛道巡线新增的控制状态在编译器栈帧中已占用 864 B，旧的
  `APP_MOTOR_TASK_STACK_DEPTH=256U` 仅提供 1 KiB，栈向下增长会覆盖相邻的静态 TCB。
  默认值已改为 `512U`（2 KiB），作为唯一的根因修复；没有变更控制逻辑、调度或硬件访问。
- 已先添加静态回归检查并确认它在旧值下失败，随后通过配置、应用集成和 Keil 集成检查。后续在获得单独授权的
  烧录/运行条件后，应通过 RTOS monitor 观察电机任务栈高水位；本轮未执行 Flash、烧录、探针、GDB/SWD
  或实车操作。

## 分层迁移事实

`mspm0g3507_app` 的规范实现位于 `app/`、`drivers/`、`algorithms/`、`protocols/`、
`services/`、`config/` 和 `platform/`。分层迁移遗留的根目录兼容头文件和 PID 旧目录的
转发头文件已经全部移除；新代码和测试必须直接引用规范路径。CCS 和 Keil 工程中每个
实现文件只加入一次。

应用配置由 `config/app_config.h` 统一维护，应用级编译宏使用 `APP_` 前缀；
`app/app_profile.h` 只保留 `app_profile_t` 和 `app_profile_get()`。编码器机械参数与解码模式
位于 `config/encoder_config.h`，RTOS monitor 资源和计时参数位于
`config/rtos_monitor_config.h`。`config/FreeRTOSConfig.h` 从 monitor 配置派生
`configMAX_TASK_NAME_LEN`，不再与服务头重复定义默认值。现有 PowerShell 构建参数名保持不变，
但直接传入旧的应用级 `-D` 宏不再是支持的接口。

`main.c` 只负责 SysConfig 初始化、调用 `app_startup()`、启动 FreeRTOS 调度器和异常
停机处理。应用任务注册接口为 `app_startup()`、`app_tasks_motor_start()`、
`app_tasks_sensor_start()`、`app_tasks_io_start()` 和 `app_tasks_telemetry_start()`。

除按键输入从旧文本 UART 报告改为独立状态快照与可选 JustFloat 遥测外，现有公开函数名、结构体名、
PowerShell 构建参数、SWD 全局变量、UART 行为和 CRSF 超时行为保持不变；
应用源码级配置宏已统一为 `APP_` 前缀。当前 `APP_IMU_YAW_ENABLE` 和 `APP_RTOS_MONITOR_ENABLE` 默认开启，
仍可分别通过 `-ImuYawEnable 0` 和 `-RtosMonitorEnable 0` 显式关闭。
分层迁移不代表任何尚未完成的硬件验收已经完成。

IMU yaw 与 IMU VOFA 遥测采用独立任务边界：`APP_IMU_YAW_ENABLE=1U` 时创建 `imu_task`，负责
BMI160 初始化、采样、失败重试和 yaw 融合；`APP_IMU_TELEMETRY_ENABLE` 默认值为 `0U`，显式设为 `1U` 时创建独立的
`imu_vofa_task`。两个任务通过 `app_state` 的 `app_imu_fusion_snapshot_t` 快照接口通信，快照包含
13 个融合诊断字段，并使用序列保护跨任务复制；BMI160 硬件诊断字段仅通过 SWD 全局变量观察。遥测帧通道依次为
`yaw_deg`、`yaw_rate_dps`、`roll_deg`、`pitch_deg`、`accel_norm_g`、`acceleration_valid`、
`gyro_bias_x_dps`、`gyro_bias_y_dps`、`gyro_bias_z_dps`、`stationary_confirmed`、`calibrated`、
`valid` 和 `dt_s`。校准或采样无效期间仍发送帧，但对应状态字段为假。
`APP_IMU_VOFA_TELEMETRY_INTERVAL_MS` 与 `APP_IMU_VOFA_TELEMETRY_TASK_STACK_DEPTH` 分别配置
独立遥测周期和栈大小；`ImuYawEnable`、`ImuTelemetryEnable` 等既有 PowerShell 参数继续保留。
由于该方案明确采用 yaw 开关控制 IMU 任务，关闭 `APP_IMU_YAW_ENABLE` 时不会初始化或读取 BMI160，
同时 `app_profile_t.enable_imu` 为 `false`；当前默认值为 `1U`，因此默认构建会创建 IMU 任务。

按键输入与遥测采用独立任务边界：`button_task` 始终运行，每 10 ms 调用
`board_buttons_scan()`，通过 `app_state_buttons_publish()` 发布四键稳定按下掩码、最近扫描周期的
按下/释放边沿掩码和序列号。未来按键控制任务与 `button_vofa_task` 均只读取该快照，不在输入任务中
实现具体业务。`APP_BUTTON_VOFA_TELEMETRY_ENABLE` 默认关闭，开启后按 `PA7、PB12、PA8、PA30`
顺序发送四通道 JustFloat，按下为 `1.0f`、释放为 `0.0f`；旧的按键功能开关、文本消息表和
`key,...` UART 输出已移除。

## 已知验证与遗留风险

### 本轮编码器方向配置与单电机开环说明

- 将四个轮位的编码器方向符号从 `drivers/encoder/board_encoder.c` 中的硬编码值移至
  `config/encoder_config.h` 的 `BOARD_ENCODER_*_DIRECTION_SIGN`；A 相双沿和 AB X4 两条
  解码路径共用这些配置，且编译期限制为 `+1` 或 `-1`。
- 当前工作区已按新硬件确认的编码器符号配置为：前左 `1`、前右 `-1`、后左 `1`、后右
  `-1`。编码器符号只修正反馈方向，不改变 PWM 电机方向映射。
- 单电机开环调试入口为构建参数 `-CrsfRemoteControlEnable 0`、变量 `g_motor_debug`、
  模式 `MOTOR_CONTROL_DEBUG_MODE_PWM` 和带符号变量 `target_duty_percent`。默认 CRSF
  构建不启用该 SWD 调试覆盖。
- 已通过 `tests\test_config_ownership.ps1`、`tests\test_motor_control.ps1 -EncoderDecodeMode 1`
  和 `tests\test_motor_control.ps1 -EncoderDecodeMode 2`。仅执行主机测试，未执行 Flash、
  烧录、探针枚举、GDB/SWD、电机调试或实物验收。

### 本轮电机 PWM 极性配置

- 新增 `config/motor_config.h`，将四轮电机正反转极性从 `board_motor.c` 的后左硬编码分支
  提取为 `BOARD_MOTOR_*_DIRECTION_SIGN`；`+1` 保持逻辑方向，`-1` 交换正反 PWM 输出。
- 根据本次开环低占空比实测，当前配置为前左 `1`、前右 `1`、后左 `-1`、后右 `1`。
  这只影响 PWM 电机方向，不影响编码器计数符号；两者需要分别配置。
- 当前工作区的 `config/crsf_config.h` 已被设为 `CRSF_REMOTE_CONTROL_ENABLE=0U`，用于无
  CRSF 的单电机 SWD 调试；原有静态集成测试仍按基线默认值 `1U` 检查，因此在该本地配置下
  会单独报告默认值不匹配。
- 未执行 Flash、烧录、探针连接或电机实物复验；需要重新构建并烧录后逐轮使用低占空比确认。

### 本轮 VS Code IntelliSense 修复

- 修复 `.vscode/settings.json` 只包含 `mspm0l1306_bringup` 头文件路径的问题，补齐四套 CCS 工程、各自 `Debug` 生成目录、SDK FreeRTOS、TI Arm Clang FreeRTOS port 和 CMSIS 路径。
- 补充 G3507 应用当前构建使用的 `__MSPM0G3507__`、`__USE_SYSCONFIG__` 和 Cortex-M0+ 编译参数，避免 CMSIS 被主机默认架构错误解析。
- 新增 `tests/test_vscode_intellisense.ps1`，检查 VS Code 配置路径、宏、编译参数和跨电脑路径可移植性。
- 根工作区的 C/C++ IntelliSense 默认面向当前主线 `mspm0g3507_app`。L1306 与 G3507 不能共用同一组芯片宏；单独检查 L1306 时应在对应工程目录或独立 VS Code 配置中选择 `__MSPM0L1306__`。
- 已验证 VS Code 配置 JSON、配置回归测试，以及使用 TI Arm Clang 对 G3507 应用入口和黑线控制源文件的 Cortex-M0+ 语法检查。
- 本轮未执行 Flash 擦除、烧录、探针枚举、GDB/SWD、电机调试或任何实物验收操作。

### 本轮 WS2812 状态指示功能

- 在 `mspm0g3507_app/config/app_config.h` 增加 `APP_WS2812_ANIMATION_ENABLE` 和
  `APP_WS2812_STATUS_INDICATOR_ENABLE`。旧的四灯轮流动画默认关闭，新的状态指示默认开启，
  两种流程不能同时启用。
- `ws2812_task` 保持单任务输出：1 号灯根据 `app_state` 中的 CRSF 链路快照显示绿灯或
  500 ms 红灯闪烁，4 号灯每 500 ms 按红、蓝、绿、白换色，2、3 号灯保持熄灭。
- 已通过 `powershell -ExecutionPolicy Bypass -File tests\test_mspm0g3507_app.ps1`。
- 已通过 `powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-app.ps1`；构建过程只生成
  `Debug` 软件产物，未执行 Flash、烧录、GDB/SWD、电机调试或 WS2812 实物验收。

### 本轮按键输入与遥测解耦

- `button_task` 已改为始终编译、初始化和创建，仅扫描按键并通过 `app_state` 发布稳定按下掩码、
  最近扫描周期的按下/释放边沿掩码和序列号；按键输入层不再访问 UART，也不包含具体业务动作。
- 新增独立 `button_vofa_task` 和 `APP_BUTTON_VOFA_TELEMETRY_ENABLE`，默认关闭；开启后按
  `PA7、PB12、PA8、PA30` 顺序发送四通道 JustFloat，按下为 `1.0f`、释放为 `0.0f`。
- 已移除旧按键功能开关、文本消息表以及 `key,...` 文本 UART 输出；按键遥测加入 UART 回显抑制和
  VOFA 编译期互斥校验，CCS/Keil 构建脚本均支持 `ButtonVofaTelemetryEnable` 临时覆盖。
- 已通过全部 `tests\*.ps1` 回归脚本，包括按键 `app_state` 主机测试、四通道 JustFloat 测试、应用/Keil
  静态集成、配置、文档、CCS 工作流和可移植性检查。
- 已通过默认配置及 `-ButtonVofaTelemetryEnable 1` 的 CCS 和 Keil 软件构建；构建只生成软件产物，
  未执行 Flash 擦除、烧录、探针枚举、GDB/SWD、电机调试或按键/VOFA 实物验收。

此前已记录通过的验证包括电机控制、编码器模式 1/2、线跟踪、IMU Yaw、CRSF、VOFA
JustFloat、G3507 应用静态集成、OLED、RTOS monitor、CCS/Keil 工程静态检查和可移植性
检查。本轮额外增加 IMU yaw app-state host test，并通过 `test_config_ownership.ps1`、`test_config_validation.ps1`、RTOS monitor
host test、CRSF、电机控制、舵机、IMU yaw、线跟踪和 VOFA 单元测试。TI Clang 与 Keil
构建结果只能说明软件构建链路通过。

本轮闭环开发已通过 `test_line_control.ps1`、`test_crsf.ps1`、`test_line_tracking.ps1`、
`test_motor_control.ps1` 和 `test_app_state_imu_yaw.ps1`。TI Clang 应用构建已分别通过默认
`CRSF_REMOTE_CONTROL_ENABLE=1` 和 `-CrsfRemoteControlEnable 0` 配置；Keil 软件工程构建也已
通过。本轮未执行 Flash、GDB、烧录、电机调试或任何循迹实物验收。

### 本轮 yaw 锁定底盘控制模式

- 新增 `CRSF_DRIVE_MODE_YAW_HOLD`：SB 中档且 SC 中档进入 yaw 锁定，SC 低档保留原手动差速，
  SC 高档安全停车，SB 高档继续黑线循迹；SC 使用通道数组索引 `7`。
- 新增 `algorithms/yaw_control/` 和可写的 `g_yaw_control_debug`。yaw 位置环按 50 ms 更新，
  左摇杆提供基础速度，输出经现有四轮速度 PID 执行；IMU 或调试参数无效时立即清零目标并复位 PID，
  yaw 跨越 ±180° 时清除微分历史。
- 已通过 `test_crsf.ps1`、`test_yaw_control.ps1`、`test_app_state_imu_yaw.ps1`、
  `test_line_control.ps1`、`test_line_tracking.ps1`、`test_motor_control.ps1`、
  `test_mspm0g3507_app.ps1`、`test_config_validation.ps1`、`test_config_ownership.ps1`、
  `test_mspm0g3507_layers.ps1` 和 `test_documentation.ps1`。
- 已通过 `tools/build-mspm0g3507-app.ps1` 和
  `tools/build-keil-mspm0g3507-app.ps1`；构建仅生成本地产物，未执行 Flash、烧录、
  探针连接、GDB/SWD、电机调试或实物验收。

本次巡线参数修订将默认 `turn_sign` 调整为 `-1.0f`，位置式 PID `kp` 调整为 `0.4f`，并将
灰度有效进入/退出阈值调整为 `800/400`。新增主机单元测试覆盖默认参数的有效判定、转向
方向和比例输出；本次验证仍仅覆盖软件行为，未执行 Flash、GDB、烧录、电机调试或实车循迹验收。

本轮 Yaw 参数修订将默认位置式 PID `kp` 调整为 `15.0f`，增加 `0.1 deg` 死区，并将转向
输出上限调整为 `700 mm/s`；主机测试补充覆盖编译期默认参数的转向输出、死区和限幅行为。

本轮未执行 Flash 擦除、烧录、探针枚举、电机调试或任何实物验收操作。巡线 VOFA 帧的实际
VOFA+ 曲线接收和 UART 物理链路仍需硬件验收。

### 本轮 HC-SR04 方案 A

- 新增 `drivers/hcsr04/` 板级驱动和 `algorithms/ultrasonic/` 纯算法模块；SysConfig 将
  `PB10` 配置为 `TRIG` 输出、`PB11` 配置为 `ECHO` 双边沿中断输入。
- 复用 `TIMG12` 10 MHz 运行时计时器测量 Echo 高电平，增加 `g_hcsr04_snapshot` 和可选
  3 通道 JustFloat 遥测；`APP_HCSR04_ENABLE` 与 `APP_HCSR04_TELEMETRY_ENABLE` 默认均为 0。
- 增加超时后的 Echo 中断 disarm、pending 清理和下一次触发前重新 arm，避免迟到边沿污染
  下一次测量。
- 已通过 `tests/test_ultrasonic_measurement.ps1`、`tests/test_hcsr04_static.ps1`、
  `tests/test_mspm0g3507_app.ps1` 和 `tests/test_rtos_monitor_static.ps1`。
- 已通过 TI Clang 和 Keil 的 `-Hcsr04Enable 1 -Hcsr04TelemetryEnable 1` 软件构建。
- 本轮仍未执行 Flash 擦除、烧录、探针连接、GDB/SWD、电机调试、示波器/逻辑分析仪测量、
  HC-SR04 接线或距离精度验收；5 V `ECHO` 分压/电平转换必须在实物测试前确认。

尚未完成或需要持续复核的硬件项目包括：

- BMI160 长期零偏、静止漂移、左右转 yaw 符号和急转弯响应；
- 编码器四轮方向、单圈计数、解码模式和低速隔离；
- 四轮速度 PID 低速调参、滤波延迟和 PWM 抖动；
- 灰度白黑归一化、位图、横向误差、全白、全黑和丢线状态；当前 `line_strength=800`
  的进入阈值允许较弱或较窄黑线进入有效状态，实际抗干扰能力仍需硬件验证；
- 黑线循迹位置式 PID 的 `turn_sign`、基础速度、四轮速度环联调和模式切换实车响应；
- yaw 锁定位置式 PID 的 `turn_sign`、目标角标定、IMU 漂移、低速响应和 SB/SC 模式切换冲击；
- HC-SR04 5 V `ECHO` 分压、电源共地、PB10/PB11 电平、近距离盲区、反射面变化和测距超时；
- HC-SR04 GPIO ISR 的进入延时差、临界区/高优先级中断影响、TIMG12 回绕和实际距离误差；
- VOFA+ 曲线数量、通道顺序、帧尾和 UART 实际接收；
- WS2812、蜂鸣器、按键和 OLED 实物响应。

### 本轮纯六轴 IMU Yaw 改造

- 删除旧的 `board_imu_yaw` 编码器辅助实现和对应主机测试，新增 `algorithms/imu_fusion/`；
  IMU 任务不再读取编码器或轮距，`app_state` 和 VOFA 现在发布完整融合诊断快照。
- BMI160 accel/gyro ODR 与 IMU 任务周期改为 200 Hz / 5 ms；任务在每次读取开始前记录 tick，并按相邻
  成功样本的 tick 差计算实际 `dt_s`。启动和请求重标定时约一秒估计三轴 gyro bias，四元数使用重力校正 roll/pitch；加速度不可信
  时回退为纯 gyro 积分。六轴 IMU 仍不能提供绝对 yaw。
- 已通过全部 `tests\*.ps1` 主机和静态测试，包括新增 `test_imu_fusion.ps1`；已通过
  `tools\build-mspm0g3507-app.ps1` 和 `tools\build-keil-mspm0g3507-app.ps1` 软件构建。
- 构建期间仅生成/更新被忽略的 `Debug`、`Generated` 和 Keil `Objects` 产物；本轮未执行 Flash、烧录、
  探针连接、GDB/SWD、电机调试或任何实车验收。SysConfig 仅输出既有信息提示，无构建错误。

### 本轮 Fusion 风格加速度方向拒绝

- 保留 BMI160 纯六轴 Mahony 融合、启动静止标定、静止 gyro bias 跟踪和既有 VOFA 帧；新增预测重力与
  测量加速度的方向误差门控。加速度模长合法但方向误差超过 `35 deg` 时不参与姿态校正，连续失配
  `3 s` 后进入恢复，防止持续拒绝导致永久失去 roll/pitch 校正。
- `acceleration_valid` 的语义更新为“当前样本实际参与重力校正”；它为假时，静止判定不会更新 gyro bias。
  未增加 VOFA 通道，`acceleration_recovery_active` 和拒绝计时仅保存在融合器状态中。
- 新增 `test_direction_error_rejects_linear_acceleration()` 与
  `test_sustained_direction_error_enters_recovery()` 主机回归用例。前者使用仍处于 `1.25 g` 模长上限内的
  `0.75 g` 横向线性加速度，验证不会错误拉偏 pitch，并验证重力方向恢复后立即重新启用校正；后者验证
  连续失配达到 `3 s` 会进入恢复。尚未执行 Flash、烧录、探针连接、GDB/SWD 或实车验收。

### 本轮 HardFault 现场捕获与 Keil 系统栈修复

- Keil 现场显示 `MSP=0x20205198` 位于启动文件原先仅 0x100 字节的异常系统栈，`LR=0xFFFFFFF1` 表明故障发生在
  异常/中断上下文，`PC=0x1` 表明返回现场已经不可信；该组合优先指向异常栈余量不足或栈帧破坏，而不是 IMU
  四元数计算本身。Keil 启动 MSP 栈已扩大到 0x400 字节，任务栈配置不变。
- `app/app_startup.c` 新增不依赖 RTOS 的 HardFault 捕获入口，现场保存到可通过 SWD 观察的
  `g_hardfault_snapshot`。重点字段是 `active`、`stacked_pc`、`stacked_lr`、`stacked_sp`、`exception_return`、
  `cfsr`、`hfsr`、`dfsr`、`mmfar`、`bfar` 和 `icsr`；再次故障时应先记录这些值再复位。
- 已通过全部 `tests\*.ps1`、TI Clang 构建和 Keil 构建。尚未执行 Flash、烧录、探针连接、GDB/SWD 或实车复验，
  因此栈扩大后的硬件复现结果仍需在目标板上确认。

### 本轮指定提交合并

- 以当前 `develop` 的 `183c378` 为共同基线，先以非快进方式合并
  `cf66bd26319dca4c1521e8726e71b285e7b65846`，生成合并提交
  `60a6e8177d2572edd6f2639c7162d39c603582da`；随后以非快进方式合并
  `a2032c0188fe77314a0cc2af3baa276ddb01bf3b`，生成合并提交
  `b92de285bd8df5b6d24168f316cca849178ee482`。两次合并均由 `ort` 策略自动完成，未出现冲突。
- 合并后的代码同时包含 BMI160 纯六轴 Mahony 融合、IMU 调试遥测与 HardFault 现场改造，以及前左/前右轮
  方向标定、后左轮速度 PID 参数修订；相关工程文档、配置所有权检查、应用静态检查和构建工程清单均随提交保留。
- 已在最终 `HEAD=b92de28` 上执行全部 `tests\*.ps1`，共 27 个脚本，`FAIL_COUNT=0`；已执行
  `tools\build-mspm0g3507-app.ps1` 和 `tools\build-keil-mspm0g3507-app.ps1`，两者退出码均为 0。
  CCS 构建完成 SysConfig 生成，Keil 构建生成 `keil/mspm0g3507_app/Objects/mspm0g3507_app.axf`；SysConfig
  仅输出既有信息提示，无构建错误。
- 本轮未执行 Flash 擦除、烧录、探针枚举、GDB/SWD 连接、电机调试、示波器/逻辑分析仪测量或任何实物验收。
  纯六轴 yaw 的长期漂移、左右转方向与急转弯响应，以及四轮方向标定和后左轮 PID 的实车表现仍需在安全条件下复核。

## STM32H723 应用基础工程

最后更新：2026-07-29

- 新增独立目录 `stm32h723_app/`。其 `stm32h723_app.ioc` 从 `D:\desktop\2026RC\Control\single_motor_test\single_motor_test.ioc` 复制后，在 STM32CubeMX 6.15.0 图形界面中裁剪并生成 `MDK-ARM` 工程。配置为 `STM32H723ZGT6`、HSE 25 MHz、550 MHz、SWD、D-Cache 关闭、FreeRTOS CMSIS-RTOS v2、UART8 `PE0/PE1` 和 UART8 TX DMA。
- FDCAN1、FDCAN2、FDCAN3 的 1 Mbit/s 时序和消息 RAM 分区保留。当前仅执行 CubeMX 初始化，不调用 `HAL_FDCAN_Start`、不发送 CAN 帧，也不包含 M2006、PID、PWM 或平衡控制。
- `App/` 提供默认关闭的 `APP_VOFA_HEALTH_TELEMETRY_ENABLE=0U`。打开后，UART8 以 1 Mbit/s 的 VOFA+ JustFloat 协议发送六通道健康帧。`volatile g_h723_debug` 面向 Keil SWD Watch 提供只读调试快照。
- 已执行 `tests\test_stm32h723_vofa_justfloat.ps1`、`tests\test_stm32h723_ioc.ps1` 和 `tests\test_stm32h723_keil_project.ps1`，均通过；`Core/` 与 `App/` 中没有 `HAL_FDCAN_Start` 或 CAN 发送调用。
- 已使用 `D:\Keil_v5\UV4\UV4.exe -r .\stm32h723_app\MDK-ARM\stm32h723_app.uvprojx -j0` 完成纯软件重建，生成 `MDK-ARM\stm32h723_app\stm32h723_app.axf`。构建日志为 `0 Error(s), 5 Warning(s)`；实际使用 Arm Compiler 6.24。若后续 Keil 再次错误地切换至 Arm Compiler 5，应先检查项目的 Arm Compiler 6 目标选择，而不要手工替换 CubeMX 生成的 FreeRTOS port。
- 未执行 Flash 擦除或烧录、探针连接、GDB/SWD 会话、UART/VOFA 实物收发、CAN 总线测试、电机测试或硬件验收。后续接入 M2006 前必须在安全条件下验证 FDCAN 引脚、时序、收发器和实际总线。

## STM32H723 CRSF 双 M2006 差速底盘

最后更新：2026-07-29

- 使用 STM32CubeMX 6.15.0 将 UART7 恢复到 `stm32h723_app/stm32h723_app.ioc`，并由 CubeMX 重新生成 MDK-ARM 工程。配置为 `PE7` RX、`PE8` TX、420000 bit/s、DMA1 Stream0 RX、DMA 与 UART7 中断；UART8 `PE1` TX 1 Mbit/s 保持不变。用于重现生成的 CubeMX 脚本为 `stm32h723_app/cubemx_generate_crsf.txt`。
- 新增 `App/app_crsf`、`app_m2006`、`app_chassis` 与 `app_chassis_service`。UART7 ReceiveToIdle DMA 回调只向软件环形缓冲写入数据；高优先级 `chassisTask` 以 1 ms 绝对节拍解析 CRSF、进行 CH3 前进/后退和 CH1 转向的差速混控、检查时效、计算增量速度 PID，并通过所选 FDCAN 发送 `0x200` 组控帧。
- M2006 总线使用 1 Mbit/s。应用启动时在选中的 FDCAN 设置标准 ID 范围过滤 `0x201..0x203`、启动控制器并订阅 FIFO0 新消息；回调排空 FIFO 并解析三台 M2006 的 8 字节反馈。左轮 ID 1 使用电流槽 1，右轮 ID 2 使用电流槽 2，ID 3 预留给上层机构。`APP_H723_M2006_FDCAN_INSTANCE` 可选择 `1U=FDCAN1 (PD0/PD1)`、`2U=FDCAN2 (PB12/PB13)` 或 `3U=FDCAN3 (PF6/PF7)`，当前默认值为 `2U`；若实物位于 FDCAN1，应改为 `1U` 后重新编译。`g_h723_debug` 已暴露选中实例、协议错误、Bus-Off 与收发错误计数，供 Keil Watch 排查无反馈。
- CRSF 仅 SB 与 SC 都为中档时进入手动模式；CRSF 有效帧超时 100 ms、任一电机反馈超时 50 ms 或开关档位不满足时均复位 PID 并发送零电流。`APP_H723_CHASSIS_ACTUATION_ENABLE` 默认 `0U`，因此即使遥控和 PID 都在运行，CAN 也只能发零电流；必须显式设为 `1U` 才可输出非零电流。
- PID 已迁移为仓库级 `shared/pid/` 纯 C 库，公共 `PID_Incremental_*` 和 `PID_Position_*` 符号保持不变。G3507 的 CCS 构建脚本、Keil 工程、源文件和主机测试均引用该路径，H723 Keil 工程也编译同一 `pid.c`。
- `volatile g_h723_debug` 现在提供 16 个 CRSF 原始通道、协议和收发错误统计、遥控目标、CAN 状态、三台 M2006 的反馈、PID 分量和电流命令，供 Keil Watch 直接观察。
- 已通过 H723 的 chassis、单电机、VOFA、CubeMX 与 Keil 工程静态测试，以及 G3507 PID、线控、航向、循迹和分层静态回归。`tools\build-mspm0g3507-app.ps1`、`tools\build-keil-mspm0g3507-app.ps1` 和 H723 Keil 构建均通过；H723 生成 `stm32h723_app.axf`，本轮构建日志为 `0 Error(s), 3 Warning(s)`，警告来自 CubeMX 生成的 FreeRTOS 未使用参数。未执行 Flash、烧录、探针连接、GDB/SWD、CRSF 实物收发、CAN 总线或电机实物测试。首次通电前必须车架悬空，确认左右方向、CAN 收发器、ID、反馈频率与 PID 参数。

## STM32H723 单个 M2006 速度环 PID 调试

`APP_H723_SINGLE_MOTOR_PID_DEBUG_ENABLE` 默认 `0U`。设为 `1U` 后，现有 1 ms `chassisTask` 完全接管 `0x200` 组控帧，Watch 输入位于 `g_h723_debug.single_motor`，包括 `enable`、`selected_id`、`target_speed_rpm` 和全部增量 PID 参数。`APP_H723_SINGLE_MOTOR_DEBUG_DEFAULT_ID` 默认 `1U`，运行时可由 Watch 覆盖为 1/2/3；ID 或 enable 切换时当前周期清零并复位 PID。非选中槽位始终为零，反馈超时 50 ms、非法参数、目标速度超过 3000 rpm 或未使能时也会复位并清零。

`APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_ENABLE` 默认 `0U`，开启后 UART8 每 1 ms 尝试发送 8 通道 JustFloat：实际目标电流、反馈电流、目标速度、反馈速度、PID 限幅前总值、P、I、D。遥测 DMA 忙时丢弃本帧并记录 UART8 丢帧计数；它与健康/JY901S 遥测编译期互斥。共享 PID 新增 `raw_output` 观测字段，但不改变 `PID_Incremental_Calc` 的原有输出限幅和控制行为。

本轮单电机功能只完成软件主机测试、静态检查和 Keil 构建，未执行 Flash、烧录、SWD、CAN 总线、电机实物或 VOFA+ 实物验收。实物调参前必须车架悬空，先确认当前默认 FDCAN2 与实际接线一致，必要时修改 `APP_H723_M2006_FDCAN_INSTANCE` 并重新构建。

## 任务完成清单

每个开发任务结束时，必须：

1. 更新本文档中的实际改动、验证命令、验证结果、硬件事实和遗留风险；
2. 功能、接口、引脚或硬件行为变化时，更新对应工程 README；
3. 工具链、SDK、CMSIS-Pack 或版本变化时，更新 `docs/DEPENDENCIES.md`，必要时更新
   `docs/SETUP.md`；
4. 构建入口、目录职责或 AI 接手流程变化时，更新根目录 `README.md`；
5. 注释规范变化时，更新 `docs/CODING_STYLE.md`；
6. 检查 Markdown 相对链接、代码围栏、失效路径和说明性英文；
7. 明确列出未执行的 Flash、烧录、GDB/SWD、电机调试和硬件验收操作。

## Git 操作授权规则

除非用户明确要求，AI 不得自行执行 `git commit`、`git push`、创建 Pull Request 或向远程
仓库发布内容。普通开发任务只允许修改工作区并运行必要的本地验证。

用户明确要求执行 `git commit` 时，提交信息必须使用详细中文，说明改动目的、主要内容
和验证结果；不得只使用笼统的英文标题或过于简短的提交说明。
