# STM32H723 Application Base

This is the independent STM32H723ZGT6 application base for the 2026 electronics design contest. Its CubeMX configuration is the source of truth for chip clocks, pins, DMA, FreeRTOS, UART8, and FDCAN.

## Scope

- MCU: `STM32H723ZGT6`.
- Toolchain: STM32CubeMX 6.15.0, STM32Cube FW_H7 V1.12.1, and Keil MDK-ARM.
- Clock: HSE 25 MHz, system clock 550 MHz.
- Debug: SWD on `PA13` and `PA14`; D-Cache is disabled.
- UART8: `PE0` RX and `PE1` TX, 8-N-1 at 1,000,000 bit/s. TX uses `DMA1_Stream1`; RX is reserved and no receive protocol is implemented.
- FreeRTOS: CMSIS-RTOS v2 default task invokes the application telemetry service.
- FDCAN: FDCAN1, FDCAN2, and FDCAN3 retain the CubeMX 1 Mbit/s timing and message RAM layout. They are initialized only. This base never calls `HAL_FDCAN_Start` and sends no CAN frame.

The intended later assignment is M2006 CAN IDs 1 and 2 for the differential-drive left/right wheels and ID 3 for the balancing mechanism. No M2006 protocol, PWM, PID, or balancing control belongs to this base.

## CubeMX Regeneration

1. Open `stm32h723_app.ioc` in STM32CubeMX.
2. Make peripheral or clock changes in CubeMX. Do not hand-edit HAL initialization code.
3. Keep the project target as `MDK-ARM`, then use `Generate Code` into this directory.
4. Preserve code only in CubeMX `USER CODE` sections. Application logic belongs in `App/`.
5. After generation, ensure the Keil project still includes `App/Src/app_debug.c`, `App/Src/app_telemetry.c`, and `App/Src/vofa_justfloat.c`, with `App/Inc` in its include paths.
6. Run the static configuration and JustFloat tests before building with Keil.

The original template was `D:\desktop\2026RC\Control\single_motor_test\single_motor_test.ioc`. UART7 and unrelated X-CUBE packages were removed during the CubeMX configuration pass.

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

## SWD Debug Snapshot

Keil Watch can directly observe the read-only-by-convention global `volatile g_h723_debug` from `App/Inc/app_debug.h`. It exposes boot count, uptime, task loop count, telemetry enable state, UART DMA start/completion/drop counters, the last HAL status, and the in-flight flag. Do not modify this variable from the debugger.

## Build And Checks

Build only; this does not program the board:

```powershell
& 'D:\Keil_v5\UV4\UV4.exe' -b '.\MDK-ARM\stm32h723_app.uvprojx' -j0
```

Expected artifact: `MDK-ARM\stm32h723_app\stm32h723_app.axf`.

Run the host and CubeMX static checks from the repository root:

```powershell
.\tests\test_stm32h723_vofa_justfloat.ps1
.\tests\test_stm32h723_ioc.ps1
```

No flash, probe connection, SWD session, CAN bus test, motor test, or hardware acceptance test is performed by these commands.
