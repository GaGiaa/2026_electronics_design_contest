# STM32H723 差速底盘应用工程

这是 2026 电赛的独立 STM32H723ZGT6 工程。芯片时钟、引脚、DMA、FreeRTOS 和 HAL 外设初始化以 `stm32h723_app.ioc` 为唯一事实来源；禁止手写或替换 CubeMX 生成的初始化代码。

## 当前范围

- MCU 为 `STM32H723ZGT6`，使用 STM32CubeMX 6.15.0、STM32Cube FW_H7 V1.12.1 和 Keil MDK-ARM。
- HSE 25 MHz，系统时钟 550 MHz；SWD 使用 `PA13/PA14`；M7 D-Cache 关闭，避免 DMA 缓冲区一致性问题。
- UART8：`PE0` RX、`PE1` TX、8-N-1、1 Mbit/s，TX 使用 `DMA1_Stream1`，用于可选 VOFA+ 健康遥测。
- UART7：`PE7` RX、`PE8` TX、8-N-1、420000 bit/s，RX 使用 `DMA1_Stream0` 的 ReceiveToIdle DMA，接收 CRSF 遥控器数据；本轮不实现 CRSF 回传。
- USART1：`PB6` TX、`PB7` RX、8-N-1、115200 bit/s，不使用 DMA；用于 BNO055 原生 UART 请求应答。
- USART2：`PD5` TX、`PD6` RX、8-N-1、234000 bit/s，RX 使用 `DMA1_Stream3` 的 ReceiveToIdle DMA，供默认关闭的 K230 钢珠位置测试链路使用。
- M2006 总线使用 1 Mbit/s，接收 ID `0x201..0x203`，每 1 ms 发送标准帧 `0x200`。`APP_H723_M2006_FDCAN_INSTANCE` 可选择 `1U=FDCAN1 (PD0/PD1)`、`2U=FDCAN2 (PB12/PB13)` 或 `3U=FDCAN3 (PF6/PF7)`；当前默认值为 `2U`。硬件必须接到所选实例对应的引脚；若实际接线位于 FDCAN1 或 FDCAN3，需同步修改该宏并重新编译。
- FreeRTOS CMSIS-RTOS v2：`chassisTask` 为高优先级 1 ms 绝对节拍任务，负责 CRSF、混控、反馈时效、增量 PID 和 CAN 组控；默认任务仍执行 UART8 遥测。

两台 M2006 的 CAN 实例由 `APP_H723_M2006_FDCAN_INSTANCE` 选择，当前默认使用 FDCAN2；左轮 ID 1、方向 `+1`，右轮 ID 2、方向 `-1`。ID 3 的上层平衡机构不属于本轮实现。

## CRSF 与安全状态机

CRSF 始终解析 16 个 11-bit 通道，CH3（数组索引 2）为前进/后退，CH1（索引 0）为左右转向。有效范围为 `172/992/1811`，归一化死区为 `0.2`。仅 SB（索引 6）和 SC（索引 7）均处于中档时进入手动差速；任一低档或高档、CRSF 超过 100 ms 未收到有效帧、任一电机反馈超过 50 ms 时都复位 PID 并下发零电流。

`App/Inc/app_config.h` 中的 `APP_H723_CHASSIS_ACTUATION_ENABLE` 默认是 `0U`。该状态下 CRSF、混控、PID 和 Watch 调试量仍会更新，但选中的 FDCAN 总线的 `0x200` 只允许发送四个零电流槽位。只有显式改为 `1U` 才会发送非零电流，首次实车前必须将车架悬空，核对 CAN 收发器、反馈 ID、左右方向宏和 PID 参数。无反馈排查时，在 Keil Watch 观察 `g_h723_debug.fdcan.instance`、`g_h723_debug.fdcan.rx_count`、`g_h723_debug.fdcan.last_status`、`g_h723_debug.fdcan.protocol_last_error`、`g_h723_debug.fdcan.protocol_bus_off`、`g_h723_debug.fdcan.tx_error_counter` 与 `g_h723_debug.fdcan.rx_error_counter`。

PID 使用仓库级 `shared/pid/` 纯 C 增量式实现，控制量统一为 M2006 减速箱输出轴 RPM，输出统一为安培。M2006 减速比为 `36:1`，C610 电流换算为 `16384 raw = 10 A`；底盘默认目标速度上限为 `550 output RPM`，电流输出上限为 `10 A`。当前底盘与单电机调试默认速度环参数为 `kp=0.25 A/RPM`、`ki=5 A/(RPM*s)`、`kd=0`、积分限幅 `100000 A`、每周期输出变化限幅 `0 A`、死区 `0.1 RPM`、积分分离阈值 `0 RPM`，这些值来自当前速度环调试配置，仍需悬空实车验证。

## 单电机速度/位置环 PID 调试

单电机调试由 `APP_H723_SINGLE_MOTOR_PID_DEBUG_ENABLE` 控制，发布配置建议为 `0U`。当前工作区为直接调试位置闭环已配置为 `1U`；启用后，单电机模式在现有 `chassisTask` 的 1 ms 节拍内完全接管 `0x200` 组控帧，CRSF 不再产生底盘差速目标；非选中电机的电流槽位始终为零。`APP_H723_SINGLE_MOTOR_DEBUG_DEFAULT_ID` 提供默认 ID（`1U..3U`），Keil Watch 中的 `g_h723_debug.single_motor.selected_id` 可以运行时覆盖它。

`control_mode=0U` 为速度模式：`target_output_speed_rpm` 与现有增量速度 PID 每 1 ms 生效。`control_mode=1U` 为串级位置模式：`target_position_deg` 是相对于使能后首帧有效反馈的输出轴连续角度，外环位置式 PID 每 `APP_H723_SINGLE_MOTOR_POSITION_PID_PERIOD_MS`（默认 5 ms）输出目标输出轴 RPM，内环仍以 1 ms 速度 PID 输出电流。M2006 的单圈编码器按 8192 counts/电机转展开为多圈位置，并按减速比 `36:1` 换算为输出轴 `deg`；位置跟踪与 `APP_H723_M2006_FEEDBACK_TIMEOUT_MS` 共用 `50 ms` 反馈有效窗口，只有达到该窗口才重新建立跟踪基准。这样可以容纳 CAN 反馈调度和 FreeRTOS tick 量化抖动，避免有效但较慢的反馈帧被误判为断流并把相对位置清零。

Watch 可直接修改速度 PID 的 `kp`、`ki`、`kd`、`output_limit`、`deadband`、`integral_output_limit`、`integral_separation_threshold`、`derivative_filter_N`、`output_delta_limit`，以及位置 PID 的 `position_kp`、`position_ki`、`position_kd`、`position_output_limit_rpm`、`position_deadband_deg`。位置 PID 默认参数为 `Kp=2`、`Ki=0`、`Kd=0`、输出限幅 `550 RPM`、死区 `0 deg`。`max_target_output_speed_rpm` 是两种模式的硬性输出轴速度上限；位置目标只要求为有限浮点值，不设置角度上限。位置反馈、原点状态、外环目标 RPM、P/I/D 分量和 5 ms 周期计数均位于 `g_h723_debug.single_motor`。

实际非零电流必须同时满足编译宏开启、`enable=1`、选中电机反馈年龄小于 50 ms、速度/位置目标及参数有效，且位置模式已建立相对零点。ID、`enable` 或 `control_mode` 切换时当前周期强制清零并复位两级 PID；位置模式随后先建立零点，再在下一次 5 ms 外环周期开始控制。单电机模式不依赖 `APP_H723_CHASSIS_ACTUATION_ENABLE`；首次调试必须车架悬空。

将 `APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_ENABLE` 设为 `1U` 可通过 UART8 以 `APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_INTERVAL_MS`（默认 1 ms）发送 JustFloat。速度模式为现有 8 通道：

1. `target_current_A`：实际下发电流，单位 A；
2. `feedback_current_A`：M2006 反馈电流，单位 A；
3. `target_output_speed_rpm`：目标输出轴转速；
4. `feedback_output_speed_rpm`：反馈输出轴转速；
5. `pid_output_A`：PID 总输出，单位 A；
6. `pid_p_out_A`：P 项输出，单位 A；
7. `pid_i_out_A`：I 项输出，单位 A；
8. `pid_d_out_A`：D 项输出，单位 A。

位置模式为 9 通道：`target_position_deg`、`feedback_position_deg`、`position_p_out_rpm`、`position_i_out_rpm`、`position_d_out_rpm`、`position_target_output_speed_rpm`、`feedback_output_speed_rpm`、`target_current_A`、`feedback_current_A`。遥测 DMA 忙时丢弃本帧并递增 UART8 丢帧计数；它与健康、JY901S 和灰度遥测编译期互斥。

该遥测宏与健康遥测和 JY901S 十通道遥测编译期互斥，三者均默认关闭。UART8 仍使用 PE1 TX、1 Mbit/s 和 `DMA1_Stream1`；DMA 忙时丢弃本周期帧并递增 UART8 丢帧计数，不阻塞控制环。

## K230 UART2 Position Test

`APP_H723_K230_UART2_TEST_ENABLE` 默认是 `0U`。设为 `1U` 后，`k230Task` 每 5 ms
解析 USART2 ReceiveToIdle DMA 接收的字节；K230 应以约 50 Hz 连续发送固定 9 字节帧：

| 偏移 | 长度 | 字段 |
| --- | --- | --- |
| 0 | 2 | 包头 `A5 5A` |
| 2 | 1 | `valid`：`0x01` 为有效距离，`0x00` 为位置丢失 |
| 3 | 4 | 小端 IEEE-754 `float32 distance_mm`，相对零点的有符号 mm |
| 7 | 2 | 小端 `CRC-16/CCITT-FALSE`，覆盖偏移 `0..6` |

`valid=0` 时 STM32 发布 `distance_mm=0.0f`；CRC 或格式错误帧不会覆盖最后一帧合法数据。
K230 `TX` 接 `PD6`，STM32 `PD5` 保留给 K230 `RX`，两端必须共地且使用 3.3 V TTL 电平。

测试宏开启时 UART8 每 `APP_H723_K230_UART2_TEST_VOFA_INTERVAL_MS`（默认 20 ms）发送
三通道 VOFA+ JustFloat：`distance_mm`、`valid`（`0.0f` 或 `1.0f`）、`frame_age_ms`。该模式与健康、JY901S、
灰度及单电机 UART8 遥测编译期互斥；UART8 保持 `PE1` TX、1 Mbit/s。Keil Watch 可观察
`g_h723_debug.ball_vision` 的距离、帧年龄、DMA 状态和 CRC/格式/UART/环形缓冲错误计数。

## CubeMX Regeneration

1. 在 STM32CubeMX 中打开 `stm32h723_app.ioc`，修改外设或时钟后生成到当前目录，工程目标保持 `MDK-ARM`。
2. 保留生成代码的 `USER CODE` 区域；应用逻辑只能放在 `App/` 或这些区域。
3. 本轮配置由 CubeMX 6.15.0 重新生成。可用 `cubemx_generate_crsf.txt` 通过 CubeMX 命令行重现生成；该脚本加载相同 `.ioc` 后执行 `project generate`。
3. 本轮配置由 CubeMX 6.15.0 重新生成。可用 `cubemx_generate_crsf.txt` 或 `cubemx_generate_k230_uart2.txt` 通过 CubeMX 命令行重现生成；脚本加载相同 `.ioc` 后执行 `project generate`。
4. 重新生成后检查 Keil 工程仍包含 `App/Src/app_debug.c`、`app_telemetry.c`、`app_jy901s.c`、`app_jy901s_service.c`、`app_bno055.c`、`app_bno055_service.c`、`app_k230.c`、`app_k230_service.c`、`app_crsf.c`、`app_m2006.c`、`app_chassis.c`、`app_chassis_service.c` 与 `../../shared/pid/pid.c`，并具有 `App/Inc` 与 `shared/pid` include 路径。

## I2C4 OLED 测试页

首版 OLED 使用 0.96 寸 128x64 SSD1306 四针 I2C 模块，接线固定为：

- `PD12` -> `I2C4_SCL`
- `PD13` -> `I2C4_SDA`
- 7-bit 地址 `0x3C`
- CubeMX I2C4 时序 `0x00B03FDB`，对应 400 kHz 快速模式

OLED 必须使用 3.3 V 供电，SCL/SDA 上拉电压不能高于 3.3 V；模块没有合适上拉时，外接约 4.7 kOhm 上拉到 3.3 V。`oledTask` 为低优先级、默认 1000 ms 周期，使用阻塞式 HAL I2C 传输和 50 ms 超时。屏幕未连接时只累计 OLED 错误并按周期重试，不会改变 FDCAN2、电机控制或 UART8 任务。

默认测试页显示 `H723 OLED TEST`、`I2C4 PD12/PD13`、`SSD1306 128X64` 和递增计数。Keil Watch 可观察 `g_h723_debug.oled` 中的初始化状态、最后 HAL 状态、初始化尝试次数、刷新次数和错误次数。该版本不读取电机实时数据。

## Runtime Speed Limit Watch Control

单电机调试模式下，Keil Watch 可修改 `g_h723_debug.single_motor.max_target_output_speed_rpm`，单位为输出轴 RPM，下一次 1 ms 内环周期生效。默认值为 `550 RPM`，由 `APP_H723_SINGLE_MOTOR_MAX_OUTPUT_RPM` 提供；该宏只决定启动默认值，不限制运行时可调范围。设为 `0`、负数或非法浮点值时，速度和位置模式均进入安全清零。

## UART8 VOFA Health Telemetry

`App/Inc/app_config.h` controls telemetry:

```c
#define APP_VOFA_HEALTH_TELEMETRY_ENABLE 0U
```

The default is off, so UART8 transmits nothing. Set the macro to `1U` to transmit a VOFA+ JustFloat frame every `APP_VOFA_HEALTH_TELEMETRY_INTERVAL_MS` (default 20 ms). Each frame has six `float` channels followed by the standard `00 00 80 7F` tail:

1. `723.0` protocol marker
2. FreeRTOS uptime in milliseconds
3. Telemetry task loop count
4. UART8 DMA start count
5. UART8 DMA completion count
6. UART8 DMA drop/failure count

Connect the USB-UART adapter GND to board GND and adapter RX to `PE1` (UART8 TX), respecting the board voltage level. Configure VOFA+ for JustFloat at 1,000,000 bit/s.

## 三按键输入与 VOFA 测试

三个按键使用高电平有效输入，接线和 GPIO 分配如下：

| 按键 | STM32 引脚 | 电气连接 |
| --- | --- | --- |
| 按键 1 | `PC5` | GPIO 外部下拉，按键另一端接 `PA4` |
| 按键 2 | `PC4` | GPIO 外部下拉，按键另一端接 `PA4` |
| 按键 3 | `PA6` | GPIO 外部下拉，按键另一端接 `PA4` |

`PA4` 配置为低速 `GPIO_MODE_OUTPUT_PP`，初始化后输出 3.3 V，作为三个按键共用的逻辑高电平源；每个
按键输入端仍需外部下拉电阻。GPIO 输入使用 `GPIO_MODE_INPUT` 和 `GPIO_NOPULL`。`PA2` 已用于灰度 ADC，
`PA5` 和 `PA7` 按板级约定保留，均不用于本按键功能。`buttonTask` 每 5 ms 采样一次，连续两次采样一致后更新稳定状态。

将 `APP_H723_BUTTON_VOFA_TELEMETRY_ENABLE` 设为 `1U` 可通过 UART8 以 20 ms 周期发送
3 通道 VOFA+ JustFloat。通道顺序固定为 `PC5`、`PC4`、`PA6`，按下输出 `1.0f`，释放输出 `0.0f`。
该宏与健康、JY901S、BNO055、灰度、单电机和 K230 UART8 遥测编译期互斥；启用按键测试前，必须
关闭其他 UART8 VOFA 模式。UART8 仍使用 `PE1` TX、1 Mbit/s 和 DMA。

## 74HC4051 Grayscale Sensor

The independent grayscale sampler uses `PG3=AD0`, `PG4=AD1`, `PG5=AD2` and
`PA2=OUT/ADC1_INP14`. `AD0` is address bit 0, `AD1` is bit 1 and `AD2` is
bit 2, so channels 0..7 use address values `000`..`111`. `PA2` is sampled
with ADC1 using software-triggered 12-bit conversions; each logical channel
is averaged from eight conversions. ADC1 runs offset and linearity calibration
during initialization. STM32H723 ADC1/ADC2 use fixed right alignment in this
HAL; therefore `adc.c` keeps `hadc1.Init.DataAlign = 0U` rather than the
unsupported `ADC_DATAALIGN_RIGHT` macro.

The ADC configuration is stored in `stm32h723_app.ioc`. After changing or
checking the pin configuration in CubeMX, click **Generate Code** so CubeMX
creates `Core/Inc/adc.h`, `Core/Src/adc.c`, updates `main.c`, enables the ADC
HAL module and registers the ADC sources in the Keil project. These generated
files are intentionally not maintained by hand in this repository; the
calibration call is in CubeMX's retained `USER CODE` block.

The grayscale task runs every 10 ms and publishes raw values, normalized
values, hysteresis digital state, black mask, line strength, line error and
ADC timeout diagnostics in `g_h723_debug.grayscale`. The current H723
white/black calibration values are `white={580,1730,730,370,2100,1580,3100,500}`
and `black={470,690,400,285,750,390,1300,300}`. Grayscale VOFA
telemetry is disabled by default; enabling
`APP_GRAYSCALE_VOFA_TELEMETRY_ENABLE` emits the same 22-channel JustFloat
layout used by the G3507 application and is mutually exclusive with the
other UART8 telemetry modes.

The software implementation does not perform Flash programming or hardware
acceptance. Verify the `000`..`111` address sequence, sensor response,
voltage range, calibration and VOFA output with the target board.

## SWD 调试快照

Keil Watch 可直接观察 `App/Inc/app_debug.h` 中的只读约定全局变量 `volatile g_h723_debug`。它按 `system`、`uart8`、`crsf`、`chassis`、`fdcan`、`m2006[3]`、`single_motor`、`jy901s`、`grayscale`、`buttons` 与 `bno055` 分组；例如 `g_h723_debug.crsf.channels_raw[0]`、`g_h723_debug.chassis.left_target_output_speed_rpm`、`g_h723_debug.m2006[0].feedback_output_speed_rpm`、`g_h723_debug.single_motor.pid_output_A`、`g_h723_debug.jy901s.angle_deg[2]`、`g_h723_debug.grayscale.line_error`、`g_h723_debug.buttons.stable_high_mask` 与 `g_h723_debug.bno055.angle_deg[2]`。`m2006` 的索引 `0/1/2` 固定对应 CAN ID `1/2/3`。快照包含底盘、单电机、灰度传感器、按键和两款 IMU 的原始、换算及通信诊断数据；由于任务和中断可独立更新字段，跨字段组合不保证为同一时刻的原子快照。

详见 [JY901S 接入说明](../docs/STM32H723_JY901S.md) 和 [BNO055 接入说明](../docs/STM32H723_BNO055.md)。

## Build And Checks

Build only; this does not program the board:

```powershell
& 'D:\Keil_v5\UV4\UV4.exe' -b '.\MDK-ARM\stm32h723_app.uvprojx' -j0
```

Expected artifact: `MDK-ARM\stm32h723_app\stm32h723_app.axf`.

从仓库根目录运行主机测试和 CubeMX 静态检查：

```powershell
.\tests\test_stm32h723_vofa_justfloat.ps1
.\tests\test_stm32h723_buttons.ps1
.\tests\test_stm32h723_buttons_static.ps1
.\tests\test_stm32h723_bno055.ps1
.\tests\test_stm32h723_chassis.ps1
.\tests\test_stm32h723_single_motor.ps1
.\tests\test_stm32h723_debug_layout.ps1
.\tests\test_stm32h723_ioc.ps1
.\tests\test_stm32h723_keil_project.ps1
```

上述命令不执行 Flash、烧录、探针连接、SWD 会话、CRSF 实物收发、CAN 总线或电机测试。实物验收仍需在车架悬空条件下进行。
