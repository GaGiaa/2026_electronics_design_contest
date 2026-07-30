# STM32H723 差速底盘应用工程

这是 2026 电赛的独立 STM32H723ZGT6 工程。芯片时钟、引脚、DMA、FreeRTOS 和 HAL 外设初始化以 `stm32h723_app.ioc` 为唯一事实来源；禁止手写或替换 CubeMX 生成的初始化代码。

## 当前范围

- MCU 为 `STM32H723ZGT6`，使用 STM32CubeMX 6.15.0、STM32Cube FW_H7 V1.12.1 和 Keil MDK-ARM。
- HSE 25 MHz，系统时钟 550 MHz；SWD 使用 `PA13/PA14`；M7 D-Cache 关闭，避免 DMA 缓冲区一致性问题。
- UART8：`PE0` RX、`PE1` TX、8-N-1、1 Mbit/s，TX 使用 `DMA1_Stream1`，用于可选 VOFA+ 健康遥测。
- UART7：`PE7` RX、`PE8` TX、8-N-1、420000 bit/s，RX 使用 `DMA1_Stream0` 的 ReceiveToIdle DMA，接收 CRSF 遥控器数据；本轮不实现 CRSF 回传。
- USART1：`PB6` TX、`PB7` RX、8-N-1、115200 bit/s，不使用 DMA；用于 BNO055 原生 UART 请求应答。
- M2006 总线使用 1 Mbit/s，接收 ID `0x201/0x202`，每 1 ms 发送标准帧 `0x200`。`APP_H723_M2006_FDCAN_INSTANCE` 可选择 `1U=FDCAN1 (PD0/PD1)`、`2U=FDCAN2 (PB12/PB13)` 或 `3U=FDCAN3 (PF6/PF7)`；当前默认值为 `2U`。若实物接线位于 FDCAN1，则将该宏改为 `1U`，重新编译即可切换。
- FreeRTOS CMSIS-RTOS v2：`chassisTask` 为高优先级 1 ms 绝对节拍任务，负责 CRSF、混控、反馈时效、增量 PID 和 CAN 组控；默认任务仍执行 UART8 遥测。

两台 M2006 实物位于 FDCAN1：左轮 ID 1、方向 `+1`；右轮 ID 2、方向 `-1`。ID 3 的上层平衡机构不属于本轮实现。

## CRSF 与安全状态机

CRSF 始终解析 16 个 11-bit 通道，CH3（数组索引 2）为前进/后退，CH1（索引 0）为左右转向。有效范围为 `172/992/1811`，归一化死区为 `0.2`。仅 SB（索引 6）和 SC（索引 7）均处于中档时进入手动差速；任一低档或高档、CRSF 超过 100 ms 未收到有效帧、任一电机反馈超过 50 ms 时都复位 PID 并下发零电流。

`App/Inc/app_config.h` 中的 `APP_H723_CHASSIS_ACTUATION_ENABLE` 默认是 `0U`。该状态下 CRSF、混控、PID 和 Watch 调试量仍会更新，但选中的 FDCAN 总线的 `0x200` 只允许发送四个零电流槽位。只有显式改为 `1U` 才会发送非零电流，首次实车前必须将车架悬空，核对 CAN 收发器、反馈 ID、左右方向宏和 PID 参数。无反馈排查时，在 Keil Watch 观察 `g_h723_debug.fdcan.instance`、`g_h723_debug.fdcan.rx_count`、`g_h723_debug.fdcan.last_status`、`g_h723_debug.fdcan.protocol_last_error`、`g_h723_debug.fdcan.protocol_bus_off`、`g_h723_debug.fdcan.tx_error_counter` 与 `g_h723_debug.fdcan.rx_error_counter`。

PID 使用仓库级 `shared/pid/` 纯 C 增量式实现，初始参数为 1 ms、`kp=1.0`、`ki=10.0`、`kd=0`、输出限幅 3000、积分限幅 1500、每周期输出变化限幅 250。这些值只是安全起点，尚未进行硬件整定。

## CubeMX Regeneration

1. 在 STM32CubeMX 中打开 `stm32h723_app.ioc`，修改外设或时钟后生成到当前目录，工程目标保持 `MDK-ARM`。
2. 保留生成代码的 `USER CODE` 区域；应用逻辑只能放在 `App/` 或这些区域。
3. 本轮配置由 CubeMX 6.15.0 重新生成。可用 `cubemx_generate_crsf.txt` 通过 CubeMX 命令行重现生成；该脚本加载相同 `.ioc` 后执行 `project generate`。
4. 重新生成后检查 Keil 工程仍包含 `App/Src/app_debug.c`、`app_telemetry.c`、`app_jy901s.c`、`app_jy901s_service.c`、`app_bno055.c`、`app_bno055_service.c`、`app_crsf.c`、`app_m2006.c`、`app_chassis.c`、`app_chassis_service.c` 与 `../../shared/pid/pid.c`，并具有 `App/Inc` 与 `shared/pid` include 路径。

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

## SWD 调试快照

Keil Watch 可直接观察 `App/Inc/app_debug.h` 中的只读约定全局变量 `volatile g_h723_debug`。它按 `system`、`uart8`、`crsf`、`chassis`、`fdcan`、`m2006[3]`、`jy901s` 与 `bno055` 分组；例如 `g_h723_debug.crsf.channels_raw[0]`、`g_h723_debug.chassis.left_target_rpm`、`g_h723_debug.m2006[0].feedback_speed_rpm`、`g_h723_debug.jy901s.angle_deg[2]` 与 `g_h723_debug.bno055.angle_deg[2]`。`m2006` 的索引 `0/1/2` 固定对应 CAN ID `1/2/3`，当前仅更新前两项，第三项为上层平衡机构预留。快照包含 16 个 CRSF 原始通道、遥控和 CAN 诊断、左右目标 RPM、两台 M2006 的反馈/PID/电流命令，以及两款 IMU 的原始、换算和通信诊断数据。不要从调试器写入；由于任务和中断可独立更新字段，跨字段组合不保证为同一时刻的原子快照。

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
.\tests\test_stm32h723_bno055.ps1
.\tests\test_stm32h723_chassis.ps1
.\tests\test_stm32h723_debug_layout.ps1
.\tests\test_stm32h723_ioc.ps1
.\tests\test_stm32h723_keil_project.ps1
```

上述命令不执行 Flash、烧录、探针连接、SWD 会话、CRSF 实物收发、CAN 总线或电机测试。实物验收仍需在车架悬空条件下进行。
