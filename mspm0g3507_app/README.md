# MSPM0G3507 WS2812 application

Independent MSPM0G3507 FreeRTOS application. UART0 uses PA10/PA11 at 115200
8-N-1. PB22 drives four 5 V WS2812 LEDs through a level shifter; all supplies
must share ground. SPI1 PICO drives PB22 at 2.666667 MHz; PB9 is the unused
SPI clock output. Each WS2812 bit is encoded as `100` for zero or `110` for
one, providing 0.375 us and 0.75 us high intervals respectively.

Build with `tools/build-mspm0g3507-app.ps1`. Output is
`mspm0g3507_app/Debug/mspm0g3507_app.out`.

The static integration test is `tests/test_mspm0g3507_app.ps1`. The animation
lights one pixel every 500 ms at channel value 16, cycling red, green, blue,
and white across pixels 1 through 4. Hardware acceptance requires observing
the LEDs and echoing continuous UART input; no Flash operation is implied by
the build or test commands.

The application CPU runs at 80 MHz from the board's 40 MHz HFXT and SYSPLL.
Four 10 kHz, two-input PWM hardware channels use PA12/PA13, PA28/PA31,
PA29/PB27, and PB4/PB5. The logical-wheel calibration maps front-left to
PA29/PB27, front-right to PB4/PB5, rear-left to PA28/PA31, and rear-right to
PA12/PA13. The logical rear-left wheel reverses the PWM input order because
its motor polarity is opposite. `BOARD_MOTOR_DIRECTION_FORWARD` therefore
means vehicle-forward motion for every logical wheel.
`board_motor_set_signed_duty()` converts signed duty commands into the existing
forward/reverse dual-PWM mapping. Positive duty means vehicle-forward motion and
negative duty means reverse. The motor supply, driver and MCU must share ground.
Hardware acceptance starts with one wheel at a low duty cycle and requires explicit
authorization before any Flash write.

Each wheel also has an AB incremental encoder. Hardware calibration found that the
physical wheel inputs do not match the original SysConfig names. The corrected
logical mapping in `mspm0g3507_app.syscfg` is front-left from PA15/PB24,
front-right from PA17/PA22 (direction inverted), rear-left from PA14/PA9, and
rear-right from PA16/PB20 (direction inverted). Inputs use
pull-ups; each A phase interrupts on both edges and the B phase determines
direction. `board_encoder` still defaults to 1040 A-phase edges per wheel
revolution (twice the reference single-edge 520 count) and a 48 mm wheel diameter;
the counts-per-revolution value remains subject to one-physical-turn measurement.
Every 10 ms motor-task iteration samples the signed encoder delta, accumulated count
and calculated mm/s speed before updating PWM. It runs one incremental PID speed
controller per wheel. `motor_pid/` is a controlled copy of only the platform-
independent PID core from the external MotorLib; CAN protocols, STM32 HAL, DJI, and
RobStride code are intentionally not included. The SWD-writable
`volatile g_motor_speed_targets_mm_s[4]` array supplies normal four-wheel mm/s
targets and starts with all targets at zero.

`volatile g_motor_debug` provides a single-wheel SWD override. Set `enable`, select
`wheel`, and choose `MOTOR_CONTROL_DEBUG_MODE_STOP`,
`MOTOR_CONTROL_DEBUG_MODE_PWM`, or `MOTOR_CONTROL_DEBUG_MODE_SPEED`. PWM mode uses
signed `target_duty_percent`; speed mode uses `target_speed_mm_per_s` and writable
`speed_pid_params` (`kp`, `ki`, `kd`, `output_limit`, `deadband`). Output is clamped
to signed 100 percent. When enabled, debug stops all non-selected wheels. Enabling
debug or changing its wheel or mode resets controller state and holds every wheel at
zero for one 10 ms control step before output resumes. Do not set breakpoints while a
motor is moving.

For observation, `g_encoder_samples[BOARD_MOTOR_COUNT]` is a volatile global
snapshot written by the 10 ms motor task and can be watched through SWD without
adding breakpoints. A lower-priority telemetry task also transmits one line every
100 ms through UART0. Each wheel is reported as
`target_mm_s,feedback_mm_s,p,i,d,duty_percent`:

```text
ctl,fl=200,181,2,0,0,2,fr=0,0,0,0,0,0,rl=0,0,0,0,0,0,rr=0,0,0,0,0,0,dbg=0,0,0,0,0,0,0
```

All values are truncated to signed integers in the serial frame. UART echo and telemetry
enqueue whole messages to a static frame queue, and one transmit task owns the
hardware FIFO so bytes from different messages cannot interleave. Telemetry does
not run in the encoder ISR or motor task. For initial validation,
use the debugger without breakpoints to turn one wheel by hand and confirm count
sign and isolation before driving the chassis at low duty.

`main.c` provides the compile-time `ENCODER_TELEMETRY_ENABLE` switch. It defaults
to `0U`; set it to `1U` to enable the periodic `ctl` UART frames and telemetry
task. Encoder sampling in the motor task, the debugger-visible sample snapshot,
IMU output, UART echo, and the UART transmit task remain enabled.

PA2 drives a passive buzzer through TIMG8 CCP1. `main.c` provides the
compile-time `BUZZER_FEATURE_ENABLE`, `BUZZER_FREQUENCY_HZ`,
`BUZZER_DUTY_PERCENT`, `BUZZER_ON_TIME_MS`, and `BUZZER_OFF_TIME_MS` macros.
The defaults are disabled, 2000 Hz, 50 percent, 200 ms on, and 1800 ms off.
When enabled, a dedicated static FreeRTOS task repeats the on/off interval;
when disabled, the PWM is initialized with a zero compare value and no buzzer
task is created. The passive buzzer driver circuit and MCU must share ground.

BMI160 uses the independent SPI0 controller. The module wiring is `SCK` to PB18,
`SDI`/MOSI to PB17, `SDO`/MISO to PB19, and active-low `CS` to PB0. PA21 is
reserved for a future data-ready interrupt and is not used by the first version.
SPI1 remains dedicated to WS2812. The BMI160 module must use 3.3 V and share
ground with the MCU; do not power both VIN and 3V3 unless the module schematic
explicitly requires it. In SPI mode, SA0 is not an address setting.

At startup the driver generates one CS low-to-high dummy SPI transaction, as
required by the Bosch reference flow to select SPI after power-up. Then
`board_bmi160.c/.h` validates the `CHIP_ID` (`0xD1`), performs the soft reset,
starts the accelerometer and gyroscope, and configures 100 Hz with ±4g and
±500dps ranges. The driver performs the official post-reset SPI communication
test and waits 1 ms after each register write. It also verifies the error,
power-mode, ODR, bandwidth, and range registers before reporting success. A
static FreeRTOS task reads the 12-byte acceleration-plus-
gyroscope register block (`0x0C` through `0x17`, gyro first) every 10 ms. The SPI controller uses Motorola mode 3 to match
the Bosch reference example. It reports `bmi160,id=...` during initialization
and compact `imu,ax=...,ay=...,az=...,gx=...,gy=...,gz=...` lines through the
existing static UART frame queue. SPI transactions have bounded timeouts;
initialization retries after one second and three consecutive read failures
trigger reinitialization.

The implementation has passed the static integration check and TI Clang build.
Hardware validation confirmed `CHIP_ID=0xD1`, approximately 1g on stationary Z
acceleration, and near-zero stationary gyroscope output. No Flash write is
performed by the build and test commands.

`main.c` provides the compile-time `IMU_TELEMETRY_ENABLE` switch. It defaults
to `0U`; set it to `1U` to enable initialization, error, and six-axis IMU UART
frames. When set to `0U`, only those IMU UART frames are disabled; the IMU task
continues to initialize BMI160, sample periodically, and retry after failures.
