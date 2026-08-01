# MSPM0 核心板工程交接索引

## H723 当前钢珠直驱与 JY901S 软下线（2026-08-01）

- 当前 ID 3 控制链路为 `K230 钢珠位置 -> 50 Hz 位置 PID -> ID3 输出轴位置 -> app_balance 位置/速度/电流级联`。`app_tilt_control`、倾角 PID、IMU 校零依赖、倾角 VOFA 及其主机测试已删除。静态和动态循迹使用独立的位置 PID 配置，默认 `position_sign=+1`；目标坐标增大时默认请求更大的 ID 3 位置。
- 保持映射由 `hold_position_mm[0..2]` 和 `hold_motor_position_deg[0..2]` 三点线性插值得到，再叠加 `pid_offset_deg`。默认坐标为 `{20, 125, 230} mm`，三点保持电机位置均为 `134 deg`，仅用于安全起步；实物调试必须在车架悬空且钢珠取出或固定时逐点实测填表。视觉失效、帧龄超过 `100 ms`、参数非法或 ID 3 不可用时清 PID 并保持最后有效 ID3 目标；尚无有效目标时保持 `134 deg`。
- ID 3 首次机械归零后自动移动至 `134 deg`。满足位置误差不超过 `1 deg`、输出轴速度绝对值不超过 `5 RPM` 并连续 `200 ms` 后直接进入 `READY`；不显示人工标定页，也不消费 PC5/PC4。移动超时、反馈失效或归零状态丢失只锁定 ID 3 为零电流，ID 1/2 和其他底盘功能保持可用。
- `APP_H723_JY901S_SERVICE_ENABLE=0U` 为默认。默认构建不调用 `MX_UART9_Init()`、不创建 JY901S 任务，也不向 JY901S 服务分发 UART9 DMA/错误回调；源码、CubeMX 配置和 Keil 项仍保留，供未来独立恢复。JY901S VOFA 不能在服务关闭时开启。
- 静态 `g_h723_debug.ball_position` 默认启用防静摩擦脉冲：误差达到驱动阈值且连续 `200 ms` 位移小于 `1 mm` 时，沿目标方向叠加 `1 deg`、持续 `60 ms` 的 ID3 位置脉冲，冷却时间为 `500 ms`。SC 高动态位置环强制关闭该功能；脉冲状态和触发计数通过静态调试快照观察，UART8 13 通道协议不变。
- UART8 钢珠 VOFA 仍为 13 通道，但第 7--9 通道改为 PID 电机位置偏移、保持电机位置和最终 ID3 目标，最后一通道为 `pipe_startup.id3_allowed`。本轮未执行 Flash、烧录、SWD/GDB、UART、K230、JY901S、CAN、电机或钢珠实物操作；历史中的倾角闭环记录仅作演进参考，不是当前运行路径。

## H723 合并任务菜单、循迹与定时任务（2026-08-01）

## H723 任务 4 距离式循迹速度曲线（2026-08-01）

- 将 `app_task4` 从纯时间-based 速度曲线替换为编码器里程-based 梯形速度曲线。
  底盘服务通过 `h723_average_distance_mm()` 取左右轮 `app_m2006_position_tracker`
  输出轴角度平均值换算为车轮圆周距离，传入 `app_task4_input_t`。任务启动时记录
  `start_distance_mm`，`step()` 中计算已驶里程，`get_output()` 中按已驶距离
  （加速段比例爬升 → 匀速段 → 制动段线性递减）输出目标速度。
- 停止条件为已驶里程 ≥ `APP_H723_TASK4_A_TO_B_DISTANCE_MM`（1500 mm），同时保留
  `APP_H723_TASK4_RUN_TIMEOUT_MS` 从 10000 ms 收紧为 8000 ms 作为安全超时。
  速度曲线中的加速和制动距离以 `APP_H723_TASK4_ACCEL_DISTANCE_MM` 和
  `APP_H723_TASK4_BRAKE_DISTANCE_MM` 配置（均为 612.5 mm：350²/(2×100)），
  全程不使用 `sqrtf`，仅用线性比例计算。
- `app_task4_start()` 签名新增 `float distance_mm` 参数以记录起点里程快照；
  `app_task4_state_t` 新增 `start_distance_mm` 和 `traveled_distance_mm`；
  `app_task4_output_t` 和 `h723_debug_task4_t` 新增 `distance_mm` 字段。
  钢球平衡由 `app_tilt_control` 在 1 ms 底盘循环中独立运行，任务 4 不做额外处理。
- 新增 `test_stm32h723_task4_integration.ps1`，检查底盘服务的距离接入、任务
  状态机集成、配置宏默认值和 debug 结构体字段；旧时间-based 测试用例已改写为
  距离-based 加速、匀速、制动和停车验证。
- 验证：全部 39 个 `tests/test_stm32h723_*.ps1` 均通过（H723_FAIL_COUNT=0）；
  Keil 纯软件重建在 stash 旧代码对比后确认 1 个 Error 为已存在的 FreeRTOS
  `port.c` 中的 `SystemCoreClock` 引用（与本轮改动无关），本轮修改的文件编译
  仅产生预期的 struct padding 警告，无新增 Error。未执行 Flash、烧录、
  SWD/GDB、CAN、编码器、电机、JY901S、灰度或实物里程/制动验收。
  首次联动应车架悬空并可立即断电，先在低占空比下确认编码器距离单位、符号和
  两组 tracker 的连续累计，再逐步提高目标速度。

## H723 合并任务菜单、循迹与定时任务（2026-08-01）

- 以 `8b87ba111e11fcabf31d808320cf9bd032d12416` 为共同基线，选择性合并无 Git 对方目录中的 H723
  任务与灰度循迹源码。导入 `app_task4`、`app_task56`、任务菜单完成接口、任务 2 简化停车逻辑、
  OLED 运行页、分组巡线 PID、任务/遥测测试和 Keil 工程源文件登记；未导入 `.o`、`.d`、`.axf`、
  `.hex`、`.map`、`.uvoptx`、`.uvguix` 或构建日志。
- 底盘服务以对方的 SE/SB/SC 状态、菜单与任务调度为主体，重新接入当前 JY901S 一致性快照、倾角 PID、
  ID 3 独占电流槽位、归零和扩展调试范围。ID 3 正常活动范围继续为 `70–210 deg`；BNO055 服务默认
  关闭，JY901S 倾角闭环的参数与保护条件保持不变。
- 任务 2 固定 `360 mm/s` 灰度循迹，`black_count >= 5` 立即停车；任务 4 使用 `350 mm/s`、
  `100 mm/s^2`、10 s 速度曲线；任务 5/6 使用 `220 mm/s`、`100 mm/s^2`、30 s 速度曲线。任务 3
  无执行器，确认后中止所有任务、清零运动路径、解除菜单锁定，OLED 显示 `TASK 3 N/A` 和
  `NOT IMPLEMENTED`。
- 底盘 VOFA 扩展为 6 通道，最后一项为当前任务运行秒数；实现与互斥检查已保留，但
  `APP_H723_CHASSIS_VOFA_TELEMETRY_ENABLE=0U`，默认 UART8 静默。遥测状态位已改为互不重叠，
  按键遥测重复配置校验已删除。
- 验证：以独立 PowerShell 进程运行全部 39 个 `tests/test_stm32h723_*.ps1`，结果均通过；执行
  `D:\Keil_v5\UV4\UV4.exe -r .\stm32h723_app\MDK-ARM\stm32h723_app.uvprojx -j0` 后，构建日志为
  `0 Error(s), 0 Warning(s)`。未执行 Flash、烧录、SWD/GDB、CAN、电机、JY901S、灰度、OLED、
  UART8/VOFA 或机械限位实物验收；首次联动应车架悬空并可立即断电。

## H723 ID 3 归零后自动进入 134 度标定姿态（2026-08-01）

- `app_pipe_startup` 在 ID 3 首次机械归零成功后，不再直接进入 `CALIBRATION_REQUIRED`，而是先进入 `MOVE_TO_CALIBRATION_POSITION`。`chassisTask` 强制既有 `app_balance` 位置/速度双环请求相对软件零位 `134.0 deg`，该请求优先于 Keil Watch 倾角目标、钢珠位置环和 `capture_zero_request`。
- 到位判据为位置误差不超过 `1.0 deg`、输出轴速度绝对值不超过 `5 RPM` 并连续 `200 ms`。满足后立即禁止 ID 3 电流并显示 OLED `CALIBRATE PIPE` 页面，用户可手动调整水管后按 PC5/B1 捕获 JY901S pitch 零位，或按 PC4/B2 放弃。移动、校准期间均屏蔽任务菜单按键，并在释放按键前维持互锁。
- 位置/速度反馈丢失、归零状态丢失或自动移动超过 `10000 ms` 会转入 `ID3_LOCKED` 并保持 ID 3 零电流；ID 1/2、遥控器和无关任务保持可用。`g_h723_debug.pipe_startup` 新增故障码、移动目标、相对位置、输出轴速度和到位稳定标志。配置位于 `APP_H723_PIPE_STARTUP_CALIBRATION_*` 宏，均为受版本管理的构建期默认值，不作为普通 Watch 写入项。
- 新增状态机单测覆盖自动进入移动、位置/速度稳定到位、超时、反馈丢失和仅锁定 ID 3；并扩展钢珠控制集成、OLED 和调试布局静态检查。未执行 Flash、烧录、SWD/GDB、CAN、电机或水管实物验收；首次实物通电仍须车架悬空、钢珠取出或固定且能立即断电。

## H723 JY901S 接管水管倾角闭环（2026-08-01）

- 水管上的 BNO055 已移出当前项目。ID 3 倾角外环现在唯一消费 JY901S UART9 服务发布的一致性控制快照；快照以序列号保护，包含校准 `vehicle_angle_deg[1]`、完整样本序号、样本时间、样本有效性和启动校准有效性。`chassisTask` 读取发布中的快照不等待，当周期按保持保护处理，绝不读取 `g_h723_debug.jy901s`。
- 倾角反馈为 `tilt_deg = -(vehicle_pitch_deg - captured_zero_deg)`，其中 `vehicle_pitch_deg=vehicle_angle_deg[1]`。用户实测校准 pitch 增大时水管倾角减小、ID 3 正转，因此外环保留位置负号映射并随 JY901S 200 Hz 完整样本以 5 ms PID 步长运行：`motor_target_deg -= pid_rate_deg_s * 0.005f`。`target_tilt_deg=+0.5f` 的预期是校准 pitch 减小、ID 3 位置减小、车尾抬高。
- 倾角闭环仅在 ID 3 已归零、ID 3 反馈新鲜、JY901S 快照年龄不超过 30 ms、`sample_valid=1`、`calibration_valid=1` 且已成功捕获水管水平零偏时激活；其余情况冻结最后安全位置并清零 PID 积分。未捕获零偏会报告 `APP_TILT_FAULT_ZERO_NOT_CAPTURED` 并保持位置，避免启动姿态偏移直接驱动 ID 3。Watch 校零、PID 参数、70–210 deg 外环限幅、13 通道倾角 VOFA 和 UART8 超时恢复机制均保留，VOFA 中的原始/年龄数据已切换到 JY901S。
- 新增 `APP_H723_BNO055_SERVICE_ENABLE`，默认 `0U`。默认构建不创建或运行 `bno055Task`，BNO055 不参与控制；源码、USART1 配置、Keil 工程项、Watch 分组、单元测试和恢复文档仍保留。若恢复 BNO055 UART8 遥测，必须同时设置该服务开关为 `1U`，编译期会检查依赖。
- 已通过全部 `tests/test_stm32h723_*.ps1` 主机/静态测试，以及 `D:\Keil_v5\UV4\UV4.exe -r .\stm32h723_app\MDK-ARM\stm32h723_app.uvprojx -j0` 纯软件重建；构建日志为 `0 Error(s), 0 Warning(s)`。未执行 Flash、烧录、SWD/GDB、UART9/JY901S 实物、UART8/VOFA、CAN、电机或水管闭环实物验收。首次通电必须车架悬空、钢珠取出或固定并可立即断电；先等待 JY901S 校准完成，再校零并从低 P 验证正目标方向。

## H723 BNO055 水管倾角闭环（2026-07-31）

- 新增纯 C `App/app_tilt_control`，作为 ID 3 机械位置环之外的约 100 Hz 倾角外环。反馈为
  校零后的 `pitch_deg`，不反相；实测 ID 3 正转、输出轴位置增大时，`pitch` 减小、车尾下降，
  因此外环以 `motor_target_deg -= pid_rate_deg_s * 0.01f` 生成位置请求。外环不直接输出电流，
  `app_balance` 仍独占归零、位置/速度 PID 和 CAN ID 3 电流槽位。
- 正常活动范围已从 `5–275 deg` 收紧为归零后 `80–180 deg`；软件 `0 deg` 仍仅用于归零机械限位。
  `g_h723_debug.tilt` 提供默认关闭的 `enable`、目标倾角、调平零偏捕获、P/I/D、死区和最大位置变化率，
  并发布样本年龄、误差、PID 分量和最终位置目标。默认参数为 10 ms、`Kp=6`、`Ki=0`、`Kd=0`、
  `0.20 deg` 死区及 `10 deg/s` 目标位置变化率。
- BNO055 服务新增非阻塞的一致性快照读取接口。未归零、ID 3 反馈不新鲜、BNO055 无效/离线、样本超过
  30 ms 或读取碰到发布中快照时，倾角外环冻结最后安全位置并清零积分；恢复后仅随新的完整样本更新。
- 已通过 `tests/test_stm32h723_tilt_control.ps1`、`tests/test_stm32h723_tilt_integration.ps1`、
  `tests/test_stm32h723_position_service.ps1` 及 ID 3、BNO055、调试布局、底盘和 Keil 工程静态测试。
  本轮未执行 Flash、烧录、SWD/GDB、CAN/BNO055/电机实物闭环验收。首次实车必须车架悬空、钢珠取出或
  固定且可立即断电；完成归零并捕获水平零偏后，从 `target_tilt_deg=0` 和低 P 开始，验证
  `target_tilt_deg=+0.5 deg` 是否让 `pitch` 增大、ID 3 位置减小、车尾抬高。

## H723 ID 3 平衡机构机械零点校准（2026-07-31）

- 新增 `App/app_balance`，独占 M2006 ID 3 的 `0x200` 第三个电流槽位；ID 1/2
  仍由原底盘或单电机调试路径控制。即使启用了单电机调试，Watch 选择 ID 3 也会
  回退到该调试通道的默认 ID，防止与平衡控制器竞争电流命令。
- 上电自动等待 ID 3 的连续反馈，随后以输出轴右手系负方向、即
  `APP_H723_BALANCE_HOME_SEARCH_OUTPUT_SPEED_RPM=-8.0f` 反转搜索机械限位。归零
  采用速度闭环和独立电流上限：搜索及确认阶段的命令电流绝不会超过
  `APP_H723_BALANCE_HOME_CURRENT_LIMIT_A=1.0f`。
- 只有输出轴速度绝对值不大于 `1 RPM`、反馈电流绝对值达到限值的 `80%`，并持续
  `300 ms`，才把当前连续多圈输出轴角度记录为软件零点。此后位置反馈为原始角度减
  该偏移；Watch 的 `g_h723_debug.balance.target_position_deg` 仅接受从零点向正方向
  的目标。`APP_H723_BALANCE_POSITION_MIN_DEG` 必须保持包含 `0 deg`，以允许建立软件零点；
  归零后的正常运行安全下限由 `APP_H723_BALANCE_POSITION_ACTIVE_MIN_DEG` 单独定义，Watch
  请求低于该值时会被限幅并在 `active_target_position_deg` 中显示。当前配置为软件零点 `0 deg`、
  正常运行下限 `80 deg`、上限 `180 deg`；其余速度和电流参数以 `app_config.h` 为准。
- `g_h723_debug.balance` 暴露 `state`、`fault`、`zero_valid`、`zero_offset_deg`、实际
  位置/速度/电流、请求与生效目标、限幅标志和命令电流；同时镜像平衡速度内环的
  PID 参数、采样周期、误差、积分状态以及 P/I/D、原始和最终输出，供 Keil Watch 排查。
  归零未完成、反馈中断、搜索
  超时或非有限位置目标都会立即清零电流并进入故障锁定；在 Watch 写入
  `rehome_request=1` 会被一次性消费，然后下一周期重新等待反馈并启动归零。
- 所有初始值均是 `app_config.h` 中可覆写、受 Git 管理的宏，不写入片内 Flash。新建
  `tests/test_stm32h723_balance.ps1` 覆盖等待反馈、负向搜限、双条件确认、零点建立、
  目标夹紧、非有限目标、超时、反馈丢失和手动重试。
- 共享 `PID_Incremental` 保留积分输出限幅、积分分离和输出饱和后的积分冻结。总输出
  `output_limit` 与可选 `output_delta_limit` 仍然生效；调参时可通过对应 PID 参数配置积分
  限幅和积分分离阈值。
- 已执行全部 `tests/test_stm32h723_*.ps1` 主机/静态测试、Keil 工程静态检查和
  `D:\Keil_v5\UV4\UV4.exe -r .\stm32h723_app\MDK-ARM\stm32h723_app.uvprojx -j0`
  纯软件重建；构建日志为 `0 Error(s), 0 Warning(s)`。本轮尚未执行 Flash、烧录、
  SWD/GDB、CAN 实车、电机、机械限位、3D 打印件受力或导轨平衡验收。首次通电必须让
  车辆悬空、可随时断电，并先确认实际“反转”确实朝向机械限位。

## VS Code 一键打开 H723 Keil 工程（2026-07-31）

- `.vscode/tasks.json` 新增 `STM32H723 App: Open Keil Project`，调用
  `tools/open-keil-stm32h723-app.ps1` 打开
  `stm32h723_app/MDK-ARM/stm32h723_app.uvprojx`。
- 启动脚本优先读取 `KEIL_ROOT` 并使用其下的 `UV4/UV4.exe`；环境变量未配置或
  对应文件不存在时回退到 `D:\Keil_v5\UV4\UV4.exe`。工程文件或 Keil 可执行文件
  缺失时会直接报错，不会静默执行其他操作。
- 本次仅新增开发入口和文档，没有执行 Keil 构建、Flash 擦除/烧录、探针枚举、
  GDB/SWD、UART、CAN、电机或其他硬件验收操作。

## H723 三按键输入与 VOFA 测试

- `stm32h723_app` 已新增三个高电平有效按键：`PC5`、`PC4`、`PA6`。`PA4` 配置为低速推挽输出并在 GPIO
  初始化后置高，作为三个按键共用的 3.3 V 逻辑电平源；三个输入使用外部下拉电阻和 `GPIO_NOPULL`，按键
  另一端接 `PA4`。`PA2` 继续用于灰度 ADC，
  `PA5` 和 `PA7` 按板级约定保留，未被本功能占用。
- `App/app_buttons` 提供按键初始化、5 ms 采样和两次一致消抖；`buttonTask` 发布原始掩码、稳定掩码和
  采样序号到 `g_h723_debug.buttons`。位 bit 0/1/2 分别对应 `PC5/PC4/PA6`，上电当前电平作为初始稳定状态。
- 新增 `APP_H723_BUTTON_VOFA_TELEMETRY_ENABLE=0U`、20 ms 发送周期和按键消抖配置。启用后 UART8
  发送 3 通道 JustFloat，顺序为 `PC5`、`PC4`、`PA6`，值为稳定状态 `0.0f/1.0f`；该模式加入现有 UART8
  遥测编译期互斥检查。
- 已通过 `tests/test_stm32h723_buttons.ps1`、`tests/test_stm32h723_buttons_static.ps1`、
  `tests/test_stm32h723_vofa_justfloat.ps1`、`tests/test_stm32h723_ioc.ps1` 和
  `tests/test_stm32h723_keil_project.ps1`。默认配置 Keil 纯软件构建和临时开启按键 VOFA、关闭单电机 VOFA
  的 Keil 纯软件构建均返回退出码 0。由于当前环境未发现 CubeMX 命令行入口，`.ioc` 与生成 GPIO 文件按现有
  CubeMX 生成格式同步修改，并由静态检查验证一致性。
- 未执行 Flash、烧录、探针枚举、GDB/SWD、按键实物电平、UART8/VOFA+ 实物接收或其他硬件验收。

最后更新：2026-07-31

## H723 K230 UART2 钢珠位置测试

- `stm32h723_app` 新增默认关闭的 `APP_H723_K230_UART2_TEST_ENABLE=0U`。启用后，USART2 使用
  `PD5=TX`、`PD6=RX`、234000 bit/s、8-N-1、DMA1 Stream3 RX 和 ReceiveToIdle DMA；K230 TX 接 PD6，
  两端必须共地并确认 3.3 V TTL 电平。`k230Task` 以 5 ms 周期从软件环形缓冲解析数据，不阻塞 UART 中断。
- K230 帧固定为 9 字节：`A5 5A`、`valid`（0 或 1）、小端 `float32 distance_mm`、小端
  `CRC-16/CCITT-FALSE`。CRC 覆盖前 7 字节。合法无效帧发布 `distance_mm=0.0f` 和 `valid=0`；CRC 或格式错误帧
  保留既有测量快照并递增错误计数。当前只提供测量测试，不接入水管倾角执行器或位置 PID。
- 测试宏同时开启 UART8 三通道 JustFloat，每 20 ms 输出 `distance_mm`、`valid`、`frame_age_ms`；它与健康、JY901S、
  灰度和单电机 UART8 遥测编译期互斥。`g_h723_debug.ball_vision` 供 Keil Watch 观察距离、帧年龄、DMA 状态与
  CRC/格式/UART/环形缓冲错误计数。
- 已通过 `tests\test_stm32h723_*.ps1` 全部 11 个 H723 主机与静态脚本，以及默认配置和临时
  `APP_H723_K230_UART2_TEST_ENABLE=1U` 的 Keil 纯软件构建。CubeMX 无界面生成命令
  `stm32h723_app\cubemx_generate_k230_uart2.txt` 已以退出码 0 验证 `.ioc`；后续在 CubeMX 图形界面重新生成时，
  应确认 USART2、DMA1 Stream3 和 NVIC 配置仍保留。
- 未执行 Flash、烧录、探针枚举、GDB/SWD、UART 实物收发、VOFA+ 实物接收、K230 识别或水管/电机硬件验收。

最后更新：2026-07-30

## H723 JY901S UART9 IMU

- STM32CubeMX 已配置并生成 UART9：`PG0=UART9_RX`、`PG1=UART9_TX`、234000 bit/s、DMA1 Stream2 RX 与 DMA/UART9 中断。
- `App/app_jy901s` 解析标准 `0x55 0x51/0x52/0x53` 帧及温度；新增纯 C `app_jy901s_calibration` 层，按车体右手系 `X=前、Y=左、Z=上` 执行正交轴映射、姿态角偏置和启动陀螺零偏校准。`jy901sTask` 只在收到新的完整样本时更新校准状态，避免重复累计同一个样本。
- 默认启动校准累计 200 个连续静止完整样本，静止门限为 `0.85~1.15 g` 和 `3 deg/s`，总超时 `5000 ms`；零偏仅保存在 RAM。`g_h723_debug.jy901s` 同时暴露原始量、`vehicle_*` 车体量、`gyro_bias_dps`、校准状态、失败原因和样本计数。安装轴索引、符号和姿态偏置位于 `stm32h723_app/App/Inc/app_config.h`。
- 当前工作区 `APP_JY901S_VOFA_TELEMETRY_ENABLE=0U`、`APP_JY901S_VOFA_CALIBRATED_ENABLE=1U`；前者默认关闭原有十通道，设为 `1U` 可开启，后者切换为车体校准结果，设为 `0U` 可恢复原始数据。UART8 遥测互斥关系保持不变；当前校准结果未接入底盘 PID。
- 已通过 JY901S 原始解析单元测试、校准模块单元测试、校准静态集成检查、debug 布局检查和 Keil 工程源文件检查。未执行 Keil 构建、烧录、探针/SWD/GDB、JY901S/VOFA 实物测试；硬件验收仍需确认实际安装轴向、静止零偏、`vehicle_acceleration_g[2]` 的重力符号以及三轴旋转方向。详细接线、单位和验收条件见 [`docs/STM32H723_JY901S.md`](STM32H723_JY901S.md)。

## H723 BNO055 USART1 IMU

- STM32CubeMX 已配置并生成 USART1：`PB6=USART1_TX`、`PB7=USART1_RX`、115200 bit/s、8-N-1；不使用 DMA 或 USART1 中断，接收 FIFO 已启用。BNO055 服务在通信错误后会清除 UART PE/FE/NE/ORE 标志并排空接收残留。BNO055 原生 UART 接线为 `PB6 -> BNO055 RX`、`PB7 <- BNO055 TX`，两端共地且 UART IO 必须为 3.3 V 兼容。
- `App/app_bno055` 是可主机测试的 BNO055 UART 寄存器协议核心。每次启动和通信恢复均先读取 `CHIP_ID=0xA0`，再按 `CONFIG -> PWR_MODE=NORMAL -> UNIT_SEL=0 -> IMU` 顺序写入并等待模式切换；每笔写入必须收到 `EE 01`，直接写 `OPR_MODE=IMU` 可能收到 `EE 03`。每笔 UART 事务超时为 20 ms，`bno055Task` 每 5 ms 执行一个请求应答步骤；当前每两个步骤完成一个样本，第一步读取 24 字节传感器块，第二步读取温度/校准并在两个系统诊断寄存器之间交替读取一个寄存器，理想完整样本周期约 10 ms（100 Hz），每个诊断寄存器约 20 ms 更新一次。这样避免在单个 5 ms 片段连续发送三次请求。错误会置 `g_h723_debug.bno055.online/sample_valid=0`，服务每 1000 ms 重试初始化。
- `g_h723_debug` 新增 `bno055` 分组，包含十项原始/换算量、校准状态、系统诊断、样本年龄和 UART/HAL 错误。`APP_BNO055_VOFA_TELEMETRY_ENABLE=0U` 默认关闭；启用后 UART8 以 20 ms 间隔发送 `Ax, Ay, Az, Gx, Gy, Gz, Roll, Pitch, Yaw, TemperatureC` 十通道 JustFloat。健康、JY901S 和 BNO055 三种 UART8 遥测编译期互斥。
- 已通过 `tests/test_stm32h723_bno055.ps1`、H723 CubeMX/调试快照/Keil 工程/RTOS 静态检查，以及 Keil 构建日志中的 `0 Error(s)`。本次实物验证已确认 BNO055 正常读取，完整样本频率接近 100 Hz；Flash、探针/SWD/GDB 和 VOFA 实物测试仍未执行。详细接线、单位和验收条件见 [`docs/STM32H723_BNO055.md`](STM32H723_BNO055.md)。

最后更新：2026-07-31

## H723 74HC4051 灰度传感器

- 已将独立灰度采样模块接入 `stm32h723_app`：`PG3=AD0`、`PG4=AD1`、`PG5=AD2`、`PA2=OUT/ADC1_INP14`。D12、D13、A8、A9 不具备本模块所需的 ADC 外部通道。
- H723 ADC 配置已经写入 CubeMX `.ioc`：ADC1、PA2/ADC1_INP14、12 位单次软件触发、轮询 EOC、较长采样时间；ADC1 初始化执行 offset+linearity 校准。ADC 的 `adc.h/adc.c` 必须由用户在 CubeMX 中点击 Generate Code 生成。H723 的 ADC1/ADC2 HAL 没有 `ADC_DATAALIGN_RIGHT` 宏，当前生成后的 `adc.c` 使用固定右对齐值 `0U`；CubeMX 重新生成后需保留该兼容修正。应用层使用生成的 `hadc1` 和 HAL ADC API。新增灰度 FreeRTOS 任务，默认周期 10 ms。
- `g_h723_debug.grayscale` 提供 8 路 raw/normalized、digital、black mask、black count、line strength、line error、ADC timeout mask、sequence 和累计超时计数。当前 H723 默认白场标定数组为 `{580,1730,730,370,2100,1580,3100,500}`，黑场标定数组为 `{470,690,400,285,750,390,1300,300}`；数组下标按 74HC4051 地址顺序对应通道 0..7。
- `APP_GRAYSCALE_VOFA_TELEMETRY_ENABLE` 默认为 `0U`；启用后 UART8 输出 22 通道灰度 JustFloat，且与健康/JY901S 遥测互斥。
- 已完成软件单元测试和 H723 IOC/Keil/debug 静态检查；尚未执行 Flash、示波器、传感器实测、VOFA 实物验证和底盘联动验收。

最后更新：2026-07-29

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

当前工作区分支为 `develop`，远端跟踪分支为 `origin/develop`。本轮以合并前的
`HEAD=e0e895d` 为基线，完成两次非快进合并后的最终合并提交为 `aedacae`；随后仅
新增本交接文档记录提交。当前分支的实际 HEAD、工作区状态和远端领先关系仍应以执行时的
`git status` 与 `git log` 输出为准。本轮巡线默认参数修订基于 `12c4258` 的历史说明保持不变。

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
- M2006 总线使用 1 Mbit/s。应用启动时在选中的 FDCAN 设置标准 ID 范围过滤 `0x201..0x203`、启动控制器并订阅 FIFO0 新消息；回调排空 FIFO 并解析三台 M2006 的 8 字节反馈。左轮 ID 1 使用电流槽 1，右轮 ID 2 使用电流槽 2，ID 3 预留给上层机构。`APP_H723_M2006_FDCAN_INSTANCE` 可选择 `1U=FDCAN1 (PD0/PD1)`、`2U=FDCAN2 (PB12/PB13)` 或 `3U=FDCAN3 (PF6/PF7)`，当前默认值按用户指定为 `2U`；硬件接线必须与该实例一致，若更换实例必须同步修改宏并重新编译。`g_h723_debug` 已暴露选中实例、协议错误、Bus-Off 与收发错误计数，供 Keil Watch 排查无反馈。
- CRSF 仅 SB 与 SC 都为中档时进入手动模式；CRSF 有效帧超时 100 ms、任一电机反馈超时 50 ms 或开关档位不满足时均复位 PID 并发送零电流。`APP_H723_CHASSIS_ACTUATION_ENABLE` 默认 `0U`，因此即使遥控和 PID 都在运行，CAN 也只能发零电流；必须显式设为 `1U` 才可输出非零电流。
- PID 已迁移为仓库级 `shared/pid/` 纯 C 库，公共 `PID_Incremental_*` 和 `PID_Position_*` 符号保持不变。G3507 的 CCS 构建脚本、Keil 工程、源文件和主机测试均引用该路径，H723 Keil 工程也编译同一 `pid.c`。
- `volatile g_h723_debug` 现在提供 16 个 CRSF 原始通道、协议和收发错误统计、遥控目标、CAN 状态、三台 M2006 的反馈、PID 分量和电流命令，供 Keil Watch 直接观察。
- 已通过 H723 的 chassis、单电机、VOFA、CubeMX 与 Keil 工程静态测试，以及 G3507 PID、线控、航向、循迹和分层静态回归。`tools\build-mspm0g3507-app.ps1`、`tools\build-keil-mspm0g3507-app.ps1` 和 H723 Keil 构建均通过；H723 生成 `stm32h723_app.axf`，本轮构建日志为 `0 Error(s), 3 Warning(s)`，警告来自 CubeMX 生成的 FreeRTOS 未使用参数。未执行 Flash、烧录、探针连接、GDB/SWD、CRSF 实物收发、CAN 总线或电机实物测试。首次通电前必须车架悬空，确认左右方向、CAN 收发器、ID、反馈频率与 PID 参数。

## STM32H723 单个 M2006 速度/位置环 PID 调试

最后更新：2026-07-30

- `APP_H723_SINGLE_MOTOR_PID_DEBUG_ENABLE` 的发布配置建议为 `0U`；当前工作区为直接调试位置闭环已配置为 `1U`。开启后 `control_mode=0U` 保持现有 1 ms 输出轴 RPM 速度环；`control_mode=1U` 启用 5 ms 基础位置式 PID 外环，其输出受 `max_target_output_speed_rpm` 和 `position_output_limit_rpm` 双重限幅后送入同一速度内环。位置目标与反馈单位均为减速后输出轴连续 `deg`。
- `app_m2006` 新增 8192 counts/转多圈展开器，FDCAN 反馈连续时累计位置；位置跟踪与 `APP_H723_M2006_FEEDBACK_TIMEOUT_MS` 共用 `50 ms` 有效窗口，只有反馈间隔达到该窗口时才重置跟踪器并使位置零点失效，避免有效但较慢的反馈帧被误判为断流。位置模式在 ID、enable 或模式变更后先发送零电流并复位两级 PID，收到新的有效反馈后把当前位置设为相对零点，再等待下一个 5 ms 周期控制。未使能、反馈超时、无效参数/目标、无效模式、未建立零点或位置 PID 状态非有限时三个 `0x200` 电流槽位均为零。
- Watch 的位置输入为 `target_position_deg` 和 `position_kp/ki/kd/output_limit_rpm/deadband_deg`；默认参数为 `Kp=2`、`Ki=0`、`Kd=0`、输出限幅 `550 RPM`、死区 `0 deg`。`feedback_position_deg`、`position_reference_valid`、外环速度目标、P/I/D、总输出和 `position_cycle_count` 用于观察。速度环参数及电流字段保留原有含义，当前默认死区为 `0.1 RPM`。
- `APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_ENABLE` 的发布配置建议为 `0U`；当前工作区已配置为 `1U`，默认发送周期为 1 ms。速度模式继续发送原有 8 通道；位置模式发送 9 通道：目标/反馈位置、位置 P/I/D、外环速度目标、内环速度反馈、目标电流和反馈电流。DMA 忙时丢帧，且与其他 UART8 遥测编译期互斥。

- 本轮深度仿真进一步复现：即使反馈间隔为 `10 ms` 且 `feedback_age_ms=0`，原 `5 ms` 位置跟踪门限仍会每帧重置跟踪器，导致 `feedback_position_deg` 持续为 `0 deg`。现已让 `APP_H723_M2006_POSITION_TRACKER_MAX_GAP_MS` 直接复用 `APP_H723_M2006_FEEDBACK_TIMEOUT_MS`，并使用严格小于比较；超过 `50 ms` 的断流仍会重建零点。当前工作区为便于本次调试显式设置了 `APP_H723_SINGLE_MOTOR_PID_DEBUG_ENABLE=1U` 和 `APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_ENABLE=1U`，该本地配置改动保留未回退。
- 本轮按用户要求将默认 `APP_H723_M2006_FDCAN_INSTANCE` 保持为 `2U`，并同步端到端仿真使用 `FDCAN2` 接收回调；硬件接线必须确认位于 FDCAN2。`tests\test_stm32h723_position_service.ps1` 覆盖 FDCAN2 接收回调、10 ms 有效慢反馈的位置累计和超过 50 ms 后的安全重建。
- 本轮验证：H723 相关单元测试、FDCAN2 位置服务集成测试、IOC/Keil/debug/VOFA/RTOS 时基检查和文档检查均通过；IOC 检查已允许单电机调试与 VOFA 宏按当前调试配置取 `0U` 或 `1U`。

- 已通过 M2006 多圈跟踪/位置换算、单电机串级调度、VOFA、debug/IOC/Keil 静态检查，以及默认与启用单电机调试宏的 Keil 纯构建。未执行 Flash、烧录、SWD、CAN 总线、电机或 VOFA+ 实物验收。实物调参前必须车架悬空，确认实际 FDCAN 实例、ID、反馈方向、编码器连续性与低增益响应。

## STM32H723 I2C4 OLED 测试页

最后更新：2026-08-01

- 已接入 0.96 寸 128x64 SSD1306 四针 I2C OLED。CubeMX 配置为 `I2C4`：`PD12=I2C4_SCL`、`PD13=I2C4_SDA`、7-bit 地址模式、模拟滤波开启、Fast Mode Plus、`I2C4.Timing=0x10A20D1F`。该 1 MHz Timing 基于 137.5 MHz I2C4 内核时钟、数字滤波 `0` 与 SCL/SDA 100 ns 上升/下降时间计算；原有 FDCAN2 `PB12/PB13` 保持不变，没有使用 `PB8/PB9`。
- `App/Src/app_oled.c` 提供阻塞式 `HAL_I2C_Master_Transmit(&hi2c4, 0x3C << 1, ...)`、128x64 framebuffer、页寻址、`0x00` 命令控制字节、`0x40` 数据控制字节、清屏、光标、字符和字符串输出、整屏刷新以及 SSD1306 初始化。`oled_font.c` 提供 0x20..0x7E 的 5x7 可打印 ASCII 字库。
- `oledTask` 使用低优先级，任务和实际刷新周期均为 100 ms，显示运行菜单、遥控状态或任务 2 状态。I2C 超时或其他总线错误只会在 `g_h723_debug.oled` 中累计并按周期重试，不会阻塞 FDCAN2、电机控制和 UART8；Watch 可观察 `initialized`、`init_attempt_count`、`last_hal_status`、`i2c_error_code`、`i2c_recovery_count`、`update_count` 和 `error_count`。
- 硬件接线要求 OLED 使用 3.3 V 供电，SCL/SDA 上拉不能高于 3.3 V。1 MHz 不属于通用 SSD1306 保证规格，实物验收必须以示波器确认两条线的上升、下降时间均不超过 100 ns，并连续观察 NACK、花屏及 OLED 错误/恢复计数；异常时将 I2C4 配置回 400 kHz。当前尚未执行烧录、示波器/I2C 波形、OLED 实物显示或电机联动验收。
- 可从 `stm32h723_app` 目录执行 `D:\STM32CubeMX\STM32CubeMX.exe -q .\cubemx_generate_crsf.txt` 重现 CubeMX 生成；生成后执行 `tests\test_stm32h723_oled.ps1`，检查 `.ioc`、`i2c.c` 中的 1 MHz Timing 和 FMP 调用，同时检查 ADC H723 的 `DataAlign=0U` 兼容修正，以及 Keil 工程仍收录 `app_oled.c`、`oled_font.c` 和 `i2c.c`。

### 本轮指定提交合并（2026-07-30）

- 以合并前 `develop` 的 `e0e895d` 为共同基线，先以 `ort` 策略非快进合并
  `11300d8b098087c8849d91d4bf60c01bc9becd4d`，生成合并提交 `b4a7368`。该合并更新
  H723 74HC4051 八路灰度传感器白场/黑场 ADC 标定数组，并同步应用代码、工程 README
  和本交接文档；未改变 ADC 通道、采样时序、归一化或底盘控制逻辑。
- 随后以 `ort` 策略非快进合并
  `721e925be9b7a2dce472f696244b69cdf72bbfcb`，生成合并提交 `aedacae`。该提交同时
  带入其父提交 `bdc5dfd` 的 H723 M2006 单电机位置环 Watch 调试基础，并保存速度环
  `deadband=0.1 RPM`、位置环 `Kp=2.0` 及对应的限幅、积分和死区参数，覆盖底盘服务、
  单电机控制、调试快照、VOFA 遥测、IOC 检查、Keil 工程静态检查和主机测试。
- 合并后已执行以下 10 个脚本，全部退出码为 0：`test_stm32h723_chassis.ps1`、
  `test_stm32h723_debug_layout.ps1`、`test_stm32h723_grayscale_math.ps1`、
  `test_stm32h723_ioc.ps1`、`test_stm32h723_jy901s.ps1`、`test_stm32h723_keil_project.ps1`、
  `test_stm32h723_rtos_timebase.ps1`、`test_stm32h723_single_motor.ps1`、
  `test_stm32h723_vofa_justfloat.ps1` 和 `test_documentation.ps1`；`git diff --check`
  无输出。另以 `D:\Keil_v5\UV4\UV4.exe -r .\stm32h723_app\MDK-ARM\stm32h723_app.uvprojx -j0`
  完成 H723 Keil 纯软件重建，退出码为 0，并生成 `stm32h723_app.axf`。
- 本轮未执行 Flash 擦除、烧录、探针枚举、GDB/SWD 连接、CAN 总线实物测试、电机调试、
  示波器/逻辑分析仪测量、VOFA+ 实物收发或灰度传感器验收。灰度标定准确性、M2006
  反馈方向、位置环增益和底盘联动仍需在车架悬空及电流输出受控的条件下复核。

## H723 任务 2 循迹停车（2026-07-31）

- 新增 `App/app_task2`：状态为 `IDLE -> DEPART -> CRUISE -> APPROACH -> STOPPED/FAULT`。SE 非高位确认任务 2 后，以 `400 mm/s` 灰度循迹；中央四路 `0x3C` 清线 30 ms 才放行，双轮反馈里程达到 `5200 mm` 后降至 `180 mm/s`，再以中央四路稳定全黑 30 ms 判定 A 点。
- 任务 2 不依赖 CRSF 摇杆。双轮反馈或灰度 ADC 失效立即故障，丢线 100 ms 或运行 20 s 超时故障；SE 高位立即中止。OLED 与 `g_h723_debug.task2` 发布阶段、故障、里程、速度和计时。任务 3 至 6 尚未实现。
- 新增任务状态机单元测试和服务/Keil 静态集成测试；实车仍需标定 `5200 mm` 减速点和 `180 mm/s` 接近速度，并连续验证停车误差不超过 2 cm、总时间不超过 20 s。

## H723 任务菜单与 CRSF 接管状态机（2026-07-31）

- `app_chassis` 新增纯 C 控制状态机。SE 使用 CRSF 数组索引 `4`，`raw >= 1300` 为按下；SB/SC 继续使用索引 `6/7`。SE 未按下时车辆进入任务菜单并保持三电机零电流；SE 按下后屏蔽按键，SB 低为遥控空闲，SB 中 + SC 低为手动底盘，SB 中 + SC 中为灰度循迹，其他挡位均安全清零。
- `chassisTask` 消费按键稳定电平的上升沿：按键 1 确认、按键 2 上移、按键 3 下移。SE 释放后菜单重置到任务 2。`app_task_menu_take_execution_request()` 提供一次性任务号接口，当前不执行任务具体流程。
- `g_h723_debug.control` 新增接管、按键、SE/SB/SC、当前任务和请求状态快照。OLED 不再显示固定测试页，始终显示任务菜单或 `REMOTE CONTROL`、`IDLE`、`MANUAL`、`LINE FOLLOW` 运行页面；已移除 `APP_H723_OLED_ENABLE` 和 `APP_H723_OLED_DEBUG_MODE_ENABLE`，刷新周期为 100 ms。
- 新增 `tests/test_stm32h723_control.ps1` 与主机状态机用例；已通过全部 H723 测试脚本（`H723_FAIL_COUNT=0`）、任务菜单、底盘、OLED、按键、debug、RTOS、IOC、Keil 工程和循迹服务检查。Keil ArmClang 6.24 纯软件构建生成 `stm32h723_app.axf`，日志为 `0 Error(s), 0 Warning(s)`；`git diff --check` 无差异错误。全仓库回归中 4 个既有 MSPM0/基线检查仍失败，原因是既有 PID 路径断言和 README 机器绝对路径，与本轮 H723 改动无关。
- 未执行 Flash、烧录、探针连接、GDB/SWD、CRSF 实物收发、CAN 总线、电机、灰度传感器或 OLED 实物验收；首次联动必须车架悬空并确认 SE/SB/SC 通道档位和电机反馈。

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

## H723 Tilt-Control VOFA Telemetry (2026-08-01)

- Added the default-off compile-time UART8 switch
  `APP_H723_TILT_CONTROL_VOFA_TELEMETRY_ENABLE=0U`. When enabled, it sends a
  13-float VOFA+ JustFloat frame every 2 ms. The fixed layout shows the full
  water-pipe outer loop and ID 3 inner-loop chain: target/calibrated tilt,
  error, P/I/D/total rate, outer position request, ID 3 position feedback and
  active target, speed target, current command, and JY901S sample age.
- The mode is included in the UART8 compile-time mutual-exclusion guard. It
  reads only `g_h723_debug` and cannot change homing, 70–210 deg position
  protection, PID behavior, current output, or CAN output.
- Validation completed: `tests\\test_stm32h723_tilt_vofa.ps1`,
  `tests\\test_stm32h723_vofa_justfloat.ps1`, and
  `tests\\test_stm32h723_chassis_telemetry.ps1` passed. No Flash, SWD, UART,
  VOFA, IMU, CAN, or motor hardware operation was performed.

## H723 UART8 VOFA DMA Recovery (2026-08-01)

- Fixed the UART8 DMA completion race: the software TX guard is reserved before
  `HAL_UART_Transmit_DMA()` starts. A completion callback that runs while the
  task is preempted can therefore no longer be overwritten by a later
  `tx_in_flight=1` store.
- Added the default `APP_H723_UART8_TX_TIMEOUT_MS=20U` watchdog. If an active
  transfer does not complete by that deadline, the default task calls blocking
  `HAL_UART_AbortTransmit()` and releases the guard only after the HAL reports
  success. The same path applies to all mutually exclusive UART8 telemetry
  modes. Watch publishes start time, timeout/recovery counters, HAL state, and
  HAL error code.
- Added pure-C guard tests for completion-during-start, start failure, and
  timeout boundaries, plus a static recovery integration check. No Flash,
  UART/VOFA physical test, IMU, CAN, or motor operation was performed.

## H723 ID 3 Extended Position Range Watch Switch (2026-08-01)

- Added `g_h723_debug.balance.allow_extended_position_range`, default `0U`.
  When set to `1U`, the manual ID 3 position target uses the separately bounded
  debug range `APP_H723_BALANCE_POSITION_DEBUG_ACTIVE_MIN_DEG=0.0f` through
  `APP_H723_BALANCE_POSITION_DEBUG_MAX_DEG=360.0f`, instead of the normal
  `70.0f–210.0f` active range. `extended_position_range_active` mirrors the
  selected range in Keil Watch.
- This is not an unlimited position override. Homing, feedback freshness,
  position/speed PID limits, current limits, CAN output protection, and the
  `0 deg` homing boundary remain active. The JY901S tilt outer loop retains its
  own normal 70–210 deg target clamp.
- Validation completed: the balance host unit test covers normal clamping,
  extended-range acceptance, and debug-maximum clamping. No Flash, SWD, CAN,
  IMU, motor, or mechanical range operation was performed.

## Git 操作授权规则

除非用户明确要求，AI 不得自行执行 `git commit`、`git push`、创建 Pull Request 或向远程
仓库发布内容。普通开发任务只允许修改工作区并运行必要的本地验证。

用户明确要求执行 `git commit` 时，提交信息必须使用详细中文，说明改动目的、主要内容
和验证结果；不得只使用笼统的英文标题或过于简短的提交说明。

## H723 Selective Merge After 805ad2a

The current H723 tree is the primary implementation after
`805ad2a9e8a39d510be543c99df854589952b152`. The partner tree had no Git
metadata, so only its core gray-line-follow and task-menu behavior was
transferred. Existing BNO055, K230 UART2, I2C4 OLED, M2006 position-loop, and
CubeMX/Keil project content were retained.

The current chassis mode table is SE pressed + SB middle + SC low for manual
mode, SE pressed + SB middle + SC middle for line follow, and remote idle for
every other SE/SB/SC state. SE not pressed selects the task menu and forces
three motor outputs to zero current.
Line follow uses a 65 mm wheel. The normalized forward stick value maps to
-300..+300 mm/s, allowing forward and reverse line following. It requires line
strength >= 800 and no ADC timeout. The gray sequence prevents repeated PID
updates, and mode exit/reset clears the PID state. The chassis debug snapshot
includes the line-follow mode, base speed, line position, validity, correction,
and final left/right RPM targets.

The current checkout explicitly sets `APP_H723_CHASSIS_ACTUATION_ENABLE=1U`.
Nonzero current commands therefore remain subject to the CRSF, switch, and
M2006 feedback safety gates. Optional five-channel chassis VOFA telemetry is
off by default via
`APP_H723_CHASSIS_VOFA_TELEMETRY_ENABLE=0U`, runs at 20 ms when enabled, and is
part of the UART8 telemetry mutual exclusion guard. `app_task_menu` is an
independent task-2-to-task-6 state machine with wraparound, 120 ms debounce,
confirm lock, reset, and display callbacks only; it has no OLED, key GPIO, or
task execution binding.

Final verification for this merge: all 18 `tests\\test_stm32h723_*.ps1`
PowerShell scripts passed; `git diff --check` passed; and the Keil software-only
build generated `stm32h723_app.axf` with `0 Error(s), 1 Warning(s)` in
`stm32h723_app\\MDK-ARM\\stm32h723_app\\stm32h723_app.build_log.htm`.
No Flash, SWD/GDB, CAN, UART/VOFA hardware, motor, grayscale sensor, OLED, or
other physical acceptance operation was performed.

## H723 Remote Dynamic Ball Line Follow (2026-08-01)

- `SE` remote takeover with `SB=middle` and `SC=high` now selects
  `APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW_BALL`. It reuses the grayscale
  line-follow controller and automatically enables a separate 40 Hz ball
  position-controller instance. `SC=middle` remains the original remote
  line-follow mode, and all task line-follow behavior is unchanged.
- `app_speed_profile` is a HAL-free 1 kHz jerk-limited speed planner. It limits
  the forward-stick request with `max_speed_mm_s`, `max_accel_mm_s2`, and
  `max_jerk_mm_s3` in `g_h723_debug.speed_profile`. Defaults are 350 mm/s,
  300 mm/s^2, and 1500 mm/s^3. Its unit test covers acceleration, braking,
  reversal, jerk/acceleration bounds, and invalid parameters.
- Dynamic ball tuning is in `g_h723_debug.ball_position_dynamic`: target mm,
  PID gains, motor-position limit, deadband, and `position_sign` (-1 or +1).
  This set is independent from the static Watch PID. Both controllers reset at
  a profile transition; the static breakaway pulse is explicitly disabled in
  the dynamic instance.
  `g_h723_debug.ball_position` and the existing 13-channel UART8 ball VOFA
  frame publish the currently active controller; `active_profile=1` means
  dynamic.
- The change passed speed-profile, control-state, ball-position, dynamic static
  integration, debug-layout, line-follow service, and Keil-project checks.
  Keil software-only rebuild was invoked successfully. Flash, SWD/GDB, UART8,
  K230, CAN, motor, steel-ball, and moving-chassis physical acceptance have not
  been performed. Begin physical validation with the chassis raised, ball
  restrained, low speed, and low position gain.

## H723 Passive Buzzer Test (2026-07-31)

The H723 application now contains an optional passive buzzer hardware test.
`PA3` is configured as `TIM2_CH4` PWM. The CubeMX source of truth is
`stm32h723_app/stm32h723_app.ioc`; the checked-in `Core/Inc/tim.h` and
`Core/Src/tim.c` implement the same CubeMX HAL initialization shape because
the local CubeMX command-line generator did not emit the newly added TIM2
files during this change.

The test is disabled by default with
`APP_H723_BUZZER_TEST_ENABLE=0U`. Define it as `1U` for a 2 kHz, 50% PWM test
that is on for 200 ms and off for 1800 ms. The default timer setup uses a 1 MHz
counter (`TIM2.Prescaler=274`, `TIM2.Period=499`). The service is called from
the existing default task and does not create another RTOS task.

Connect the passive buzzer signal to `PA3` and its return to board ground only
after checking the buzzer current. Use a transistor or MOSFET driver when the
load exceeds the STM32 GPIO rating. Static checks and software-only Keil builds
were run; Flash, SWD, oscilloscope, audible buzzer, and other physical
acceptance tests remain outstanding.

## H723 巡线位置式 PID Watch 与 VOFA 调试（2026-07-31）

本轮新增 `g_h723_debug.line_follow` 运行时调试接口，服务于 SE 按下、SB
中档、SC 中档的灰度巡线模式。灰度任务周期为 `10 ms`，底盘任务周期为
`1 ms`；巡线位置式 PID 只在灰度 `sequence` 变化时调用，因此实际 PID
计算频率约为 `100 Hz`，初始化 `dt_s=0.010 s`。重复灰度快照沿用上一次
转向修正；ADC 超时、线强度低于 `800` 或快照无效时复位 PID 并输出零目标。

Keil Watch 可直接修改以下字段，并在下一次 PID 计算前生效：

- `pid_kp`、`pid_ki`、`pid_kd`
- `pid_output_limit_mm_s`
- `pid_deadband`
- `reset_pid_request`，写入 `1` 后清除 PID 状态并自动恢复为 `0`

默认参数为 `Kp=35.0`、`Ki=0`、`Kd=0`、输出限幅
`APP_H723_LINE_FOLLOW_MAX_TURN_SPEED_MM_S`、死区 `0`。非有限参数、负增益、
负死区或非正输出限幅会被拒绝，上一组合法参数继续使用；`params_valid` 和
`params_rejected_count` 用于观察校验结果。快照还提供 `line_position`、
`error`、`integral`、`p_out`、`i_out`、`d_out`、`raw_output`、`pid_output`、
`turn_correction_mm_s`、基础速度、左右轮目标速度、`line_strength`、
`sequence` 和 `pid_update_count`。

新增 `APP_H723_LINE_FOLLOW_PID_VOFA_TELEMETRY_ENABLE=0U`，周期为 `10 ms`。
启用后 UART8 发送 13 通道 JustFloat，顺序为：

1. `line_position`
2. `error`
3. `p_out`
4. `i_out`
5. `d_out`
6. `raw_output`
7. `pid_output`
8. `turn_correction_mm_s`
9. `base_speed_mm_s`
10. `left_target_speed_mm_s`
11. `right_target_speed_mm_s`
12. `line_strength`
13. `sequence`

该遥测已加入 UART8 遥测互斥编译检查，只用于调试观察，不改变 CRSF
安全门、M2006 速度环或 CAN 输出。本轮已通过巡线 PID 静态检查、底盘遥测、
VOFA JustFloat、debug layout 检查和 Keil 纯软件构建；构建日志为
`0 Error(s), 0 Warning(s)`。未执行 Flash、烧录、SWD/GDB、UART/VOFA 实物、
CAN、电机或灰度传感器硬件验收。

## H723 钢珠位置闭环与水管 pitch 上电校准（2026-08-01）

- 新增 `App/app_ball_position_control` 纯 C 位置式 PID。控制周期固定为 `25 ms`（40 Hz），误差为
  `target_mm - measured_mm`，默认符号为 `-1`：目标坐标增大时请求负水管倾角，目标坐标减小时请求正水管倾角。
  默认 `Kp=0.02 deg/mm`、`Ki=0`、`Kd=0`、死区 `1 mm`、目标倾角限幅 `+/-3 deg`，
  由 `g_h723_debug.ball_position` 在 Keil Watch 调整；`enable` 默认 `0U`。
- 新增 `App/app_pipe_startup` 状态机。ID 3 首次归零成功后进入 OLED `CALIBRATE PIPE` 页面；PC5/B1
  在 JY901S 样本有效且不超时的条件下捕获当前 pitch，PC4/B2 放弃并锁定 ID 3 零电流，PA6 忽略。
  `READY` 或 `ID3_LOCKED` 均返回原遥控器/任务页面；放弃、归零失败和未归零只影响 ID 3，其他两电机仍可运行。
- `APP_H723_K230_UART2_ENABLE=1U` 使 UART2 K230 接收链路默认启用；原
  `APP_H723_K230_UART2_TEST_ENABLE=0U` 仅保留测试 VOFA 遥测。K230 服务新增序列保护快照接口，位置环
  使用最新有效距离，帧龄超过 `100 ms`、视觉无效、ID 3 未校准或反馈故障时清 PID 并请求 0 度校准倾角。
- 位置环只生成 `target_tilt_deg`，仍经 JY901S 5 ms 倾角环和 `app_balance` 的归零、70--210 deg 范围、
  速度/电流/CAN 安全保护；新增 `g_h723_debug.ball_position` 和 `pipe_startup` Watch 快照以及 Keil 工程源文件登记。
- 位置环新增三点分段线性 `hold_tilt` 补偿、`engage/release` 误差滞回、按请求倾角方向的起动补偿，
  以及只在 K230 新帧到来时更新的低通速度阻尼。补偿、起动与速度增益默认均为零，保留原有控制行为；
  Watch 可调参数均会校验有限值、递增坐标、非负增益和合法滞回后才生效。
- 已通过位置 PID、启动状态机、K230 UART2 静态和球控集成静态测试；随后重新执行全部 42 个
  `tests\test_stm32h723_*.ps1`，均通过。`git diff --check` 无差异错误；以
  `D:\Keil_v5\UV4\UV4.exe -r .\stm32h723_app\MDK-ARM\stm32h723_app.uvprojx -j0`
  完成 H723 纯软件重建，日志为 `0 Error(s), 0 Warning(s)`。
- 尚未执行 Flash、烧录、SWD/GDB、UART/VOFA 实物、K230 识别、CAN、电机、机械限位或 OLED 实物验收。

## H723 钢珠位置 PID VOFA 遥测（2026-08-01）

- 新增默认关闭的 `APP_H723_BALL_POSITION_VOFA_TELEMETRY_ENABLE=0U`。打开后 UART8 以
  `25 ms` 间隔发送 13 通道 VOFA+ JustFloat：目标/反馈/误差 mm、P/I/D、PID 输出、目标倾角、
  视觉帧龄/有效性、位置环状态/故障和启动校准有效性。
- 该模式复用既有 UART8 DMA 发送、发送超时恢复和 `g_h723_debug` 快照，只读且不改变位置 PID、
  JY901S 倾角环、归零、电流或 CAN 输出；它与所有其他 UART8 VOFA 模式编译期互斥。
- 已通过新增的 `tests\test_stm32h723_ball_position_vofa.ps1`、全部 43 个 H723 测试脚本、
  文档检查和 `git diff --check`。默认关闭及临时启用该开关的两次 Keil 纯软件构建均为
  `0 Error(s), 0 Warning(s)`；未执行 UART8/VOFA 或其他实物验收。

## H723 钢珠位置环摩擦与弯管补偿（2026-08-01）

- `app_ball_position_control` 保持 40 Hz 位置 PID，新增三点分段线性保持倾角、双阈值滞回、
  正/负倾角独立起动补偿和 K230 新帧速度低通阻尼。默认保持表为 0 deg、起动补偿为 0 deg、
  速度增益为 0，因此未通过 Watch 调参时保留原控制行为。
- Keil Watch 使用 `hold_position_mm[0..2]` 递增定义 K230 坐标，`hold_tilt_deg[0..2]` 填入各点
  钢珠静止时的目标倾角；`engage_error_mm` 必须不小于 `release_error_mm`。`velocity_mm_s` 为朝车尾
  的正方向，正 `velocity_gain_deg_per_mm_s` 产生相反重力方向的制动倾角。`drive_active`、
  `hold_tilt_output_deg`、`breakaway_tilt_output_deg` 和 `velocity_damping_tilt_deg` 用于区分各分量。
- 已通过扩展后的钢珠位置单元/集成测试、全部 43 个 H723 测试脚本、`git diff --check` 和 Keil
  纯软件构建，日志为 `0 Error(s), 0 Warning(s)`。当前工作区将
  `APP_H723_BALL_POSITION_VOFA_TELEMETRY_ENABLE` 设为 `1U` 以便现场观察；发布配置应改回 `0U`。
  未执行 Flash、烧录、K230、UART8/VOFA、CAN、电机或钢珠实物验收。
