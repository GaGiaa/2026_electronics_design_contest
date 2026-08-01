# STM32H723 差速底盘应用工程

这是 2026 电赛的独立 STM32H723ZGT6 工程。芯片时钟、引脚、DMA、FreeRTOS 和 HAL 外设初始化以 `stm32h723_app.ioc` 为唯一事实来源；禁止手写或替换 CubeMX 生成的初始化代码。

## 当前范围

- MCU 为 `STM32H723ZGT6`，使用 STM32CubeMX 6.15.0、STM32Cube FW_H7 V1.12.1 和 Keil MDK-ARM。
- HSE 25 MHz，系统时钟 550 MHz；SWD 使用 `PA13/PA14`；M7 D-Cache 关闭，避免 DMA 缓冲区一致性问题。
- UART8：`PE0` RX、`PE1` TX、8-N-1、1 Mbit/s，TX 使用 `DMA1_Stream1`，用于可选 VOFA+ 健康遥测。
- UART7：`PE7` RX、`PE8` TX、8-N-1、420000 bit/s，RX 使用 `DMA1_Stream0` 的 ReceiveToIdle DMA，接收 CRSF 遥控器数据；本轮不实现 CRSF 回传。
- USART1：`PB6` TX、`PB7` RX、8-N-1、115200 bit/s，不使用 DMA；保留给默认停用的 BNO055 原生 UART 服务。
- USART2：`PD5` TX、`PD6` RX、8-N-1、234000 bit/s，RX 使用 `DMA1_Stream3` 的 ReceiveToIdle DMA，默认接入 K230 钢珠位置反馈链路。
- M2006 总线使用 1 Mbit/s，接收 ID `0x201..0x203`，每 1 ms 发送标准帧 `0x200`。`APP_H723_M2006_FDCAN_INSTANCE` 可选择 `1U=FDCAN1 (PD0/PD1)`、`2U=FDCAN2 (PB12/PB13)` 或 `3U=FDCAN3 (PF6/PF7)`；当前默认值为 `2U`。硬件必须接到所选实例对应的引脚；若实际接线位于 FDCAN1 或 FDCAN3，需同步修改该宏并重新编译。
- FreeRTOS CMSIS-RTOS v2：`chassisTask` 为高优先级 1 ms 绝对节拍任务，负责 CRSF、混控、反馈时效、增量 PID 和 CAN 组控；默认任务仍执行 UART8 遥测。

两台 M2006 的 CAN 实例由 `APP_H723_M2006_FDCAN_INSTANCE` 选择，当前默认使用 FDCAN2；左轮 ID 1、方向 `+1`，右轮 ID 2、方向 `-1`。ID 3 由独占的平衡机构控制器使用，不参与底盘差速混控或单电机调试通道。

## ID 3 钢珠位置直驱

`app_balance` 始终独占 ID 3 的机械归零、输出轴位置环、速度环和 `0x200` 第三个电流槽位。归零后，正常位置目标限制在相对软件零位 `70–210 deg`；`0 deg` 只保留给负向搜索机械限位。钢珠位置控制不再经过水管倾角或 IMU 外环：静态 Watch 环和动态循迹环均以 50 Hz 直接生成 ID 3 输出轴位置请求，随后仍由 `app_balance` 的位置、速度、电流级联和机械保护执行。

首次归零成功后，`app_pipe_startup` 强制 ID 3 自动移动至 `134 deg`。位置误差不超过 `1 deg`、速度绝对值不超过 `5 RPM` 且持续 `200 ms` 后直接进入 `READY`；无需人工倾角校零，也不消费 PC5/PC4。自动移动超时、反馈失效或归零状态丢失只锁定 ID 3 为零电流，ID 1/2、遥控器和无关任务仍可用。

钢珠位置 PID 的目标和反馈都使用 K230 坐标系的 mm，误差为 `target_mm - measured_mm`。其输出单位为 ID 3 输出轴 `deg`，由三点 `hold_position_mm[0..2]` / `hold_motor_position_deg[0..2]` 线性插值得到保持位置后，叠加 `Kp`、`Ki`、`Kd` 产生的 `pid_offset_deg`。静态和动态配置分别位于 `g_h723_debug.ball_position` 与 `g_h723_debug.ball_position_dynamic`；二者的 `position_sign` 均可在 Watch 设为 `+1/-1`，默认 `+1`，即目标坐标增大时请求更大的 ID 3 位置。

默认保持表坐标为 `{20, 125, 230} mm`，三点电机位置均为 `134 deg`。这只是安全起步值，不代表实际平衡映射；必须在车架悬空、钢珠取出或固定的情况下，逐点测得钢珠静止时的 ID 3 位置后再通过 Watch 填入表项。默认 PID 为 `Kp=0.02 deg/mm`、`Ki=Kd=0`、死区 `1 mm`、偏移限幅 `+/-3 deg`。视觉无效、帧龄超过 `100 ms`、参数非法或 ID 3 不可用时，控制器清 PID 并保持最后一个有效 ID 3 目标；尚无有效目标时保持 `134 deg`，不会退回机械限位。

为核对机构行程，Keil Watch 可将 `g_h723_debug.balance.allow_extended_position_range` 设为 `1U`。默认 `0U` 保持正常 `70–210 deg`；设为 `1U` 后，手动 `balance.target_position_deg` 切换到仍有边界的 `0–360 deg` 调试范围。此开关不会绕过归零、反馈超时、PID 限速/限流或 CAN 电流保护；只有在已确认调试范围不会撞击机构时才能使用。

UART8 发送以“先预占、再启动 DMA”的方式消除完成回调与任务抢占的竞态。若单次传输超过
`APP_H723_UART8_TX_TIMEOUT_MS`（默认 `20 ms`），默认任务会调用 `HAL_UART_AbortTransmit()`
恢复 UART8。Watch 中 `g_h723_debug.uart8.tx_timeout_count`、`tx_recovery_count`、
`tx_recovery_failure_count`、`hal_g_state` 和 `hal_error_code` 用于判断恢复是否发生。当前
`2 ms` 周期为 500 Hz 调试配置；只有持续观察到 `tx_timeout_count=0` 时才应保留该频率，常规调参
建议使用 `10 ms`。

## 任务 2：循迹一圈并停回 A 点

SE 未按下且菜单空闲时底盘保持零输出。按键 1 确认任务 2 后，`App/app_task2` 状态机复用 8 路灰度循迹：以 `400 mm/s` 驶离 A 点，中央四路掩码 `0x3C` 连续 30 ms 清线后进入巡航；左右轮实际反馈积分达到 `5200 mm` 后降至 `180 mm/s`；再次连续 30 ms 检测到中央四路全黑时锁止停车并保留总用时。

任务运行不依赖 CRSF 摇杆，但要求左右轮反馈新鲜、灰度 ADC 正常且循迹线有效。反馈或 ADC 失效立即进入 `FAULT`；连续丢线 100 ms 或运行 20 s 超时也会锁止停车。SE 切到高位会立即中止任务并交回遥控。OLED 和 `g_h723_debug.task2` 显示阶段、故障、里程、速度及用时。任务3使用独立钢珠位置状态机；任务4至6保持原有底盘执行器行为。

## 任务 3：钢珠平衡流程

确认任务3后立即使用 K230 毫米位置闭环，初始目标为 `125 mm`。B1 第一次按下后目标切换为
`225 mm` 并开始计时；当有效视觉位置达到或超过 `175 mm` 时自动切换为 `75 mm`。B1 第二次按下
结束任务并冻结计时，之后恢复通用钢珠目标。视觉数据无效时保持现有安全目标，不触发阈值切换。

菜单末页 `BALL SET` 不是运动任务。钢珠放在目标位置后按 B1，只有有效且未超时的视觉样本才会更新
`g_h723_debug.ball_position.target_mm`；失败时保留旧目标并显示 `CAPTURE FAIL`。任务模式（SE 未按下）
始终运行 ID 3 钢珠闭环，任务3期间临时覆盖通用目标；遥控器模式（SE 按下）只有 SB 中档允许闭环，
SB 低档、高档或异常档位强制 ID 3 零电流。

## CRSF 与安全状态机

CRSF 始终解析 16 个 11-bit 通道。CH3（数组索引 2）为左摇杆前后，CH1（索引 0）为右摇杆左右；有效范围为 `172/992/1811`，归一化死区为 `0.2`。SE 使用索引 4，`raw >= 1300` 表示按下；SB 使用索引 6，SC 使用索引 7。

SE 未按下时进入任务菜单/任务模式，CRSF 不接管车辆，ID 1/2 按任务状态运行，ID 3 始终使用通用或任务3钢珠目标闭环。SE 按下后屏蔽按键并进入遥控接管：SB 中档允许 ID 3 使用 Watch 通用目标，SB 低档、高档及其他挡位将 ID 3 置零；SC 继续选择手动底盘或灰度循迹模式。CRSF 超过 100 ms 未收到有效帧、任一电机反馈超过 50 ms 或安全挡位不满足时都会复位 PID 并下发零电流。

底盘控制状态机位于 `App/app_chassis` 纯 C 模块。`chassisTask` 消费 CRSF 和按键稳定状态，OLED 任务只读取 `g_h723_debug.control` 和 `g_h723_debug.chassis` 显示状态，不参与电机决策。

当前 `App/Inc/app_config.h` 显式将 `APP_H723_CHASSIS_ACTUATION_ENABLE` 设为 `1U`。因此，满足全部运行安全条件时，选中的 FDCAN 总线可以发送非零电流。首次实车前必须将车架悬空，核对 CAN 收发器、反馈 ID、左右方向宏和 PID 参数。将该宏设为 `0U` 时，CRSF、混控、PID 和 Watch 调试量仍会更新，但 `0x200` 只允许发送四个零电流槽位。无反馈排查时，在 Keil Watch 观察 `g_h723_debug.fdcan.instance`、`g_h723_debug.fdcan.rx_count`、`g_h723_debug.fdcan.last_status`、`g_h723_debug.fdcan.protocol_last_error`、`g_h723_debug.fdcan.protocol_bus_off`、`g_h723_debug.fdcan.tx_error_counter` 与 `g_h723_debug.fdcan.rx_error_counter`。

PID 使用仓库级 `shared/pid/` 纯 C 增量式实现，控制量统一为 M2006 减速箱输出轴 RPM，输出统一为安培。M2006 减速比为 `36:1`，C610 电流换算为 `16384 raw = 10 A`；底盘默认目标速度上限为 `550 output RPM`，电流输出上限为 `10 A`。当前底盘与单电机调试默认速度环参数为 `kp=0.25 A/RPM`、`ki=5 A/(RPM*s)`、`kd=0`、积分限幅 `100000 A`、每周期输出变化限幅 `0 A`、死区 `0.1 RPM`、积分分离阈值 `0 RPM`，这些值来自当前速度环调试配置，仍需悬空实车验证。

## 三按键任务菜单

按键 1（PC5）为确认，按键 2（PC4）为上移，按键 3（PA6）为下移。按键任务每 5 ms 采样，连续两次一致后发布稳定电平；底盘任务只处理稳定电平的上升沿。菜单显示任务2~6和末页 `BALL SET`，确认后锁定并发布一次性请求，可通过 `app_task_menu_take_execution_request()` 取走任务号。任务3内部只使用B1推进流程；`BALL SET` 的B1采样当前视觉位置。

SE 按下时按键完全屏蔽；SE 从按下释放后菜单重置到任务 2 并重新开放按键。任务菜单和 CRSF 接管不能同时控制车辆。

## 单电机速度/位置环 PID 调试

单电机调试由 `APP_H723_SINGLE_MOTOR_PID_DEBUG_ENABLE` 控制，发布配置建议为 `0U`。当前工作区为直接调试位置闭环已配置为 `1U`；启用后，单电机模式在现有 `chassisTask` 的 1 ms 节拍内完全接管 `0x200` 组控帧，CRSF 不再产生底盘差速目标；非选中电机的电流槽位始终为零。`APP_H723_SINGLE_MOTOR_DEBUG_DEFAULT_ID` 提供默认 ID（`1U..3U`），Keil Watch 中的 `g_h723_debug.single_motor.selected_id` 可以运行时覆盖它。

`control_mode=0U` 为速度模式：`target_output_speed_rpm` 与现有增量速度 PID 每 1 ms 生效。`control_mode=1U` 为串级位置模式：`target_position_deg` 是相对于使能后首帧有效反馈的输出轴连续角度，外环位置式 PID 每 `APP_H723_SINGLE_MOTOR_POSITION_PID_PERIOD_MS`（默认 5 ms）输出目标输出轴 RPM，内环仍以 1 ms 速度 PID 输出电流。M2006 的单圈编码器按 8192 counts/电机转展开为多圈位置，并按减速比 `36:1` 换算为输出轴 `deg`；位置跟踪与 `APP_H723_M2006_FEEDBACK_TIMEOUT_MS` 共用 `50 ms` 反馈有效窗口，只有达到该窗口才重新建立跟踪基准。这样可以容纳 CAN 反馈调度和 FreeRTOS tick 量化抖动，避免有效但较慢的反馈帧被误判为断流并把相对位置清零。

Watch 可直接修改速度 PID 的 `kp`、`ki`、`kd`、`output_limit`、`deadband`、`integral_output_limit`、`integral_separation_threshold`、`derivative_filter_N`、`output_delta_limit`，以及位置 PID 的 `position_kp`、`position_ki`、`position_kd`、`position_output_limit_rpm`、`position_deadband_deg`。位置 PID 默认参数为 `Kp=20`、`Ki=0`、`Kd=0`、输出限幅 `550 RPM`、死区 `0 deg`。`max_target_output_speed_rpm` 是两种模式的硬性输出轴速度上限；位置目标只要求为有限浮点值，不设置角度上限。位置反馈、原点状态、外环目标 RPM、P/I/D 分量和 5 ms 周期计数均位于 `g_h723_debug.single_motor`。

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

## JY901S 软下线

JY901S 模块源码、UART9 CubeMX 配置、Keil 工程项及说明均被保留，但当前默认由
`APP_H723_JY901S_SERVICE_ENABLE=0U` 软下线。默认构建不会调用 `MX_UART9_Init()`、不会创建
JY901S FreeRTOS 任务，也不会将 UART9 DMA 或错误回调分发给该服务；JY901S 数据不参与 ID 3 或钢珠控制。
恢复模块时必须将该宏设为 `1U` 并重新完成独立验证。JY901S VOFA 还要求服务开关同时开启，避免默认软下线时误打开遥测。详细的可恢复模块说明见
[`docs/STM32H723_JY901S.md`](../docs/STM32H723_JY901S.md)。

## K230 UART2 钢珠位置闭环

`APP_H723_K230_UART2_ENABLE` 默认是 `1U`，`k230Task` 每 5 ms
解析 USART2 ReceiveToIdle DMA 接收的字节；K230 应以约 50 Hz 连续发送固定 9 字节帧：

| 偏移 | 长度 | 字段 |
| --- | --- | --- |
| 0 | 2 | 包头 `A5 5A` |
| 2 | 1 | `valid`：`0x01` 为有效距离，`0x00` 为位置丢失 |
| 3 | 4 | 小端 IEEE-754 `float32 distance_mm`，相对零点的有符号 mm |
| 7 | 2 | 小端 `CRC-16/CCITT-FALSE`，覆盖偏移 `0..6` |

`valid=0` 时 STM32 发布 `distance_mm=0.0f`；CRC 或格式错误帧不会覆盖最后一帧合法数据。
K230 `TX` 接 `PD6`，STM32 `PD5` 保留给 K230 `RX`，两端必须共地且使用 3.3 V TTL 电平。

仅当测试遥测宏 `APP_H723_K230_UART2_TEST_ENABLE=1U` 时，UART8 每 `APP_H723_K230_UART2_TEST_VOFA_INTERVAL_MS`（默认 20 ms）发送
三通道 VOFA+ JustFloat：`distance_mm`、`valid`（`0.0f` 或 `1.0f`）、`frame_age_ms`。该模式与健康、JY901S、
灰度及单电机 UART8 遥测编译期互斥；UART8 保持 `PE1` TX、1 Mbit/s。Keil Watch 可观察
`g_h723_debug.ball_vision` 的距离、帧年龄、DMA 状态和 CRC/格式/UART/环形缓冲错误计数。

钢珠位置 PID 固定为 50 Hz（20 ms）。静态环由 `g_h723_debug.ball_position.enable` 显式启用；动态遥控循迹模式使用独立的 `g_h723_debug.ball_position_dynamic` 配置。两者均直接输出 `target_motor_position_deg`，不依赖 JY901S。三点保持表和 PID 偏移的调参方式见前文“ID 3 钢珠位置直驱”。

静态环默认启用防静摩擦脉冲：当误差达到驱动阈值且视觉坐标连续 `200 ms` 内位移小于 `1 mm` 时，沿目标方向叠加默认 `1 deg`、持续 `60 ms` 的 ID3 位置脉冲，随后冷却 `500 ms`。可在 `g_h723_debug.ball_position` 调整 `breakaway_enable`、脉冲幅值、停滞判定、持续时间和冷却时间；`breakaway_active`、`breakaway_trigger_count`、`breakaway_stall_elapsed_ms` 和 `breakaway_offset_deg` 用于观察运行状态。动态 SC 高模式强制关闭该脉冲，避免把静态调参带入行驶控制。

`APP_H723_BALL_POSITION_VOFA_TELEMETRY_ENABLE=1U` 时，UART8 按 50 Hz 发送固定 13 通道 VOFA+ JustFloat 帧：`target_mm`、`measured_mm`、`error_mm`、P/I/D、`pid_offset_deg`、`target_motor_position_deg`、`vision_age_ms`、`vision_valid`、位置环 `state`、`fault` 与 `pipe_startup.id3_allowed`。保持电机位置仍可通过 `g_h723_debug.ball_position.hold_motor_position_output_deg` 在 Watch 观察。当前配置为 `1U`；该模式与其他 UART8 VOFA 遥测编译期互斥，只读调试快照。

## CubeMX Regeneration

1. 在 STM32CubeMX 中打开 `stm32h723_app.ioc`，修改外设或时钟后生成到当前目录，工程目标保持 `MDK-ARM`。
2. 保留生成代码的 `USER CODE` 区域；应用逻辑只能放在 `App/` 或这些区域。
3. 本轮配置由 CubeMX 6.15.0 重新生成。可用 `cubemx_generate_crsf.txt` 通过 CubeMX 命令行重现生成；该脚本加载相同 `.ioc` 后执行 `project generate`。
3. 本轮配置由 CubeMX 6.15.0 重新生成。可用 `cubemx_generate_crsf.txt` 或 `cubemx_generate_k230_uart2.txt` 通过 CubeMX 命令行重现生成；脚本加载相同 `.ioc` 后执行 `project generate`。
4. 重新生成后检查 Keil 工程仍包含 `App/Src/app_debug.c`、`app_telemetry.c`、`app_jy901s.c`、`app_jy901s_service.c`、`app_bno055.c`、`app_bno055_service.c`、`app_k230.c`、`app_k230_service.c`、`app_crsf.c`、`app_m2006.c`、`app_chassis.c`、`app_chassis_service.c` 与 `../../shared/pid/pid.c`，并具有 `App/Inc` 与 `shared/pid` include 路径。

## I2C4 OLED 运行页面

首版 OLED 使用 0.96 寸 128x64 SSD1306 四针 I2C 模块，接线固定为：

- `PD12` -> `I2C4_SCL`
- `PD13` -> `I2C4_SDA`
- 7-bit 地址 `0x3C`
- CubeMX I2C4 使用 Fast Mode Plus：`I2C4.Timing=0x10A20D1F`、`I2C4.FASTMODEPLUS=I2C_FASTMODEPLUS_I2C4`，目标 1 MHz。该 Timing 基于 137.5 MHz I2C4 内核时钟、模拟滤波开启、数字滤波 `0`、SCL/SDA 100 ns 上升/下降时间计算。

OLED 必须使用 3.3 V 供电，SCL/SDA 上拉电压不能高于 3.3 V。1 MHz 时必须按实际总线电容选取上拉并用示波器确认上升、下降时间不超过 100 ns；常见 SSD1306 模块只保证 400 kHz，因此出现 NACK、花屏或 `i2c_recovery_count` 增长时必须回退到 400 kHz。`oledTask` 为低优先级，每 100 ms 唤醒且每 100 ms 刷新一次，使用阻塞式 HAL I2C 传输和 50 ms 超时。屏幕未连接时只累计 OLED 错误并按周期重试，不会改变 FDCAN2、电机控制或 UART8 任务。OLED 始终作为应用服务运行，不使用 OLED enable 或 debug 编译宏。每次 CubeMX 重生成后运行 `tests\\test_stm32h723_oled.ps1`，确认 `.ioc`、`i2c.c` 的 Timing 和 FMP 调用仍一致。

任务菜单页面显示 `TASK MENU`、当前任务号以及按键提示；任务3页面显示阶段、目标mm、视觉位置和计时；`BALL SET` 页面显示通用目标、视觉位置和采样结果；SE 接管时显示 `REMOTE CONTROL`、`IDLE`、`MANUAL` 或 `LINE FOLLOW`，并显示 SB/SC 档位和 CRSF 帧年龄。详细诊断数据仍通过 `g_h723_debug` 提供给 Keil Watch，不通过 OLED 宏切换页面。

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

## Passive Buzzer Test

The optional passive buzzer test uses `PA3` as `TIM2_CH4` PWM. The default
configuration is silent:

```c
#define APP_H723_BUZZER_TEST_ENABLE 0U
```

Build with `APP_H723_BUZZER_TEST_ENABLE=1U` to enable a 2 kHz, 50% PWM output.
The application drives the output for 200 ms and stops it for 1800 ms in a
loop. The timer counter is 1 MHz (`TIM2.Prescaler=274`, default period 499).
Connect a passive buzzer between `PA3` and board ground only when the buzzer
current is within the MCU GPIO rating. Use a transistor or MOSFET driver for
any higher-current load. This software change has not been Flash-programmed or
accepted with a scope or physical buzzer.

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
.\tests\test_stm32h723_control.ps1
.\tests\test_stm32h723_bno055.ps1
.\tests\test_stm32h723_chassis.ps1
.\tests\test_stm32h723_single_motor.ps1
.\tests\test_stm32h723_debug_layout.ps1
.\tests\test_stm32h723_ioc.ps1
.\tests\test_stm32h723_buzzer.ps1
.\tests\test_stm32h723_keil_project.ps1
```

上述命令不执行 Flash、烧录、探针连接、SWD 会话、CRSF 实物收发、CAN 总线或电机测试。实物验收仍需在车架悬空条件下进行。

## Selective Merge: Gray Line Follow And Task Menu

This checkout keeps the H723 implementation after baseline commit
`805ad2a9e8a39d510be543c99df854589952b152` as the primary implementation.
The partner-only additions are integrated as independent application modules;
the existing BNO055, K230 UART2, I2C4 OLED, M2006 position loop, and safety
behavior remain in place.

Chassis switch mapping in the current control state machine is:

- SE not pressed: task menu and three motor outputs at zero current.
- SE pressed, SB low: remote idle and three motor outputs at zero current.
- SE pressed, SB middle and SC low: manual chassis mode.
- SE pressed, SB middle and SC middle: gray line-follow mode.
- SE pressed, SB middle and SC high: dynamic ball line-follow mode.
- All other switch combinations: remote idle and three motor outputs at zero current.

Line follow uses the 65 mm wheel diameter. The normalized forward stick value
maps to -350..+350 mm/s, allowing forward and reverse line following. A
line-strength value below 800, any ADC timeout, or a missing gray sample stops
the output. The gray sequence prevents repeated PID
updates, and exiting/resetting the mode clears the PID state. The line-follow
debug snapshot exposes mode, base speed, line position, validity, turn
correction, and final left/right target RPM values.

## Static Ball Position Tuned Defaults

The default stationary `g_h723_debug.ball_position` Watch values now match the
latest tuning snapshot: enabled with `target_mm=125`, `Kp=2.0`, `Ki=0.0`,
`Kd=0.8`, `output_limit_deg=360`, `pid_deadband_mm=0.5`,
`engage_error_mm=0.5`, `release_error_mm=0.5`, and `position_sign=1.0`.
The breakaway pulse is disabled by default; its retained configuration is
`10 deg`, `200 ms`, `1 mm`, `60 ms`, and `500 ms`.
The three-point hold mapping remains `{20, 125, 230} mm` to `{134, 134, 134}
deg` until measured values are supplied through Watch.

## Remote Dynamic Ball Line Follow

With `SE` pressed, `SB` in the middle position, and `SC` high, the chassis
uses the normal grayscale line-follow controller while the forward-stick speed
is planned at 1 kHz before it reaches the line-follow base-speed input. The
planner limits speed, acceleration, and jerk, so stick changes, release, and
forward/reverse transitions do not create an instantaneous wheel-speed step.

`g_h723_debug.speed_profile` exposes `max_speed_mm_s`, `max_accel_mm_s2`, and
`max_jerk_mm_s3` as Watch inputs. `requested_speed_mm_s`,
`planned_speed_mm_s`, and `planned_accel_mm_s2` show the active trajectory.
Defaults are `350 mm/s`, `300 mm/s^2`, and `1500 mm/s^3` respectively.
Writing `reset_request=1` clears the trajectory state.

This mode automatically selects `g_h723_debug.ball_position_dynamic` rather
than the stationary `g_h723_debug.ball_position.enable` path. Set the desired
K230-frame coordinate with `ball_position_dynamic.target_mm`; tune
`pid_kp`, `pid_ki`, `pid_kd`, `output_limit_deg`, and `pid_deadband_mm`
independently. `position_sign` must be exactly `-1.0f` or `1.0f`; it defaults
to `1.0f`. Static and dynamic position loops have separate PID, hysteresis,
and measurement histories. The static loop's breakaway pulse is disabled for
this dynamic instance. `g_h723_debug.ball_position.active_profile`
is `1` for the dynamic loop and `0` for the stationary loop, so the existing
13-channel ball-position VOFA frame continues to show whichever loop is active.

The dynamic loop remains subject to the same ID3 homing, K230 validity, and
100 ms vision-age gates. A failed gate clears the active position PID and holds
the last safe motor-position target. Initial vehicle testing must be performed
with the chassis lifted and the ball retained, beginning with low speed and low
PID gain; this software was not flashed or physically accepted during development.

This checkout explicitly sets `APP_H723_CHASSIS_ACTUATION_ENABLE=1U`; nonzero
current commands are therefore possible once every CRSF, switch, and feedback
safety gate is satisfied. Optional chassis VOFA telemetry is controlled by
`APP_H723_CHASSIS_VOFA_TELEMETRY_ENABLE=0U`, uses five channels at 20 ms, and is
included in the UART8 telemetry mutual-exclusion check.

`app_task_menu` is a standalone task-2-to-task-6 selection state machine. It
provides page wraparound, 120 ms key debounce, confirm locking, reset, a
one-shot execution-request API, and a callback-based display adapter. The H723
chassis control state machine binds it to the three debounced buttons and the
SE takeover switch. OLED rendering reads the published control/debug snapshot.

The merge was verified with all 18 `tests\\test_stm32h723_*.ps1` scripts,
`git diff --check`, and a Keil software-only build. The build log reports
`stm32h723_app.axf` with `0 Error(s), 1 Warning(s)`. No Flash programming,
SWD/GDB session, CAN, UART/VOFA hardware, motor, grayscale sensor, OLED, or
other physical acceptance test was performed.

## 巡线位置式 PID Watch 与 VOFA 调试

SB 中档、SC 中档时进入灰度巡线模式。灰度任务每 `10 ms` 发布一次新快照，底盘任务虽然每 `1 ms` 运行，但巡线位置式 PID 只在灰度 `sequence` 变化时计算，因此实际 PID 计算频率约为 `100 Hz`，PID `dt_s` 为 `0.010 s`。灰度序号未变化时沿用上一次转向修正；ADC 超时、线强度小于 `800` 或快照无效时复位 PID 并输出零目标。

`g_h723_debug.line_follow` 提供运行时调参。Keil Watch 可直接修改 `pid_kp`、`pid_ki`、`pid_kd`、`pid_output_limit_mm_s` 和 `pid_deadband`，下一次 PID 计算前生效。当前默认值为 `35.0f`、`0.0f`、`0.0f`、`APP_H723_LINE_FOLLOW_MAX_TURN_SPEED_MM_S` 和 `0.0f`。参数无效时保留上一组合法参数，并通过 `params_valid`、`params_rejected_count` 报告；写入 `reset_pid_request=1` 可清除积分和历史误差，程序随后自动清零该请求。

该调试快照还提供 `line_position`、`error`、`integral`、`p_out`、`i_out`、`d_out`、`raw_output`、`pid_output`、`turn_correction_mm_s`、基础速度、左右轮目标速度、`line_strength`、`sequence` 和 `pid_update_count`，用于区分灰度输入、PID 分量、输出限幅和底盘目标生成问题。

将 `APP_H723_LINE_FOLLOW_PID_VOFA_TELEMETRY_ENABLE` 改为 `1U` 可通过 UART8 以 10 ms 周期发送 13 通道 JustFloat；该宏与其他 UART8 遥测互斥。通道顺序为：`line_position`、`error`、`p_out`、`i_out`、`d_out`、`raw_output`、`pid_output`、`turn_correction_mm_s`、`base_speed_mm_s`、`left_target_speed_mm_s`、`right_target_speed_mm_s`、`line_strength`、`sequence`。本功能只用于观察，不改变 CRSF 安全门、M2006 速度环或 CAN 输出逻辑。
