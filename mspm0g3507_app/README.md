# MSPM0G3507 WS2812 application

Independent MSPM0G3507 FreeRTOS application. UART0 uses PA10/PA11 at 115200
8-N-1. PB22 drives four 5 V WS2812 LEDs through a level shifter; all supplies
must share ground. SPI1 PICO drives PB22 at 2.666667 MHz; PB9 is the unused
SPI clock output. Each WS2812 bit is encoded as `100` for zero or `110` for
one, providing 0.375 us and 0.75 us high intervals respectively.

Build with `tools/build-mspm0g3507-app.ps1`. Output is
`mspm0g3507_app/Debug/mspm0g3507_app.out`.

## CCS project and portable build

The `.project`, `.cproject`, and `.ccsproject` files are kept for CCS source
navigation, SysConfig editing, and target/debug configuration. The FreeRTOS
kernel sources are installed inside the local MSPM0 SDK rather than stored in
this repository, so the imported CCS project does not describe a complete
standalone application build. Clicking `Project > Build Project` in a fresh
workspace can therefore report `FreeRTOS.h` or `pid.h` not found, and a stale
CCS workspace can also select an older SDK or SysConfig installation.

Use the repository build script for a reproducible application build. It adds
the SDK FreeRTOS include and port directories, the repository `motor_pid`
include directory, and all required FreeRTOS kernel sources:

```powershell
powershell -ExecutionPolicy Bypass -File tools\configure-toolchain.ps1 -PersistUserEnvironment
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-app.ps1
```

If the CCS GUI must build the application, configure that local project with
the same SDK FreeRTOS include/port directories and `motor_pid` include
directory, then add `list.c`, `queue.c`, `tasks.c`, `portable\TI_ARM_CLANG\ARM_CM0\port.c`,
and `portasm.c` from that SDK to the project and link their objects. This is a
per-computer CCS workspace configuration; do not replace it with absolute
paths in the repository.

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
direction. `board_encoder` declares the wheel motor mechanics explicitly: a 13-line
encoder on the motor shaft and a 20:1 gearbox derive 260 output-shaft lines per
wheel revolution. The default `BOARD_ENCODER_DECODE_MODE_A_PHASE_DUAL_EDGE` counts
both A-phase edges and therefore derives 520 counts per output-shaft revolution.
Set the compile-time `BOARD_ENCODER_DECODE_MODE` to
`BOARD_ENCODER_DECODE_MODE_AB_PHASE_QUADRATURE_X4` to count both edges of both AB
phases through the quadrature state decoder; it then derives 1040 counts per output
shaft revolution. Rebuild and flash after changing the mode, then verify one manual
output-shaft turn at low speed before using the speed loop. The wheel diameter is
48 mm.
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
`speed_pid_params` (`kp`, `ki`, `kd`, `output_limit`, `deadband`). By default, speed
debug uses the selected wheel's entry in `g_default_speed_pid_params`; set
`use_speed_pid_override` to `true` to explicitly use the values in `speed_pid_params`
for the selected wheel. Output is clamped to signed 100 percent. When enabled, debug stops all non-selected wheels. Enabling
debug or changing its wheel or mode resets controller state and holds every wheel at
zero for one 10 ms control step before output resumes. Do not set breakpoints while a
motor is moving.

For observation, `g_encoder_samples[BOARD_MOTOR_COUNT]` is a volatile global
snapshot written by the 10 ms motor task and can be watched through SWD without
adding breakpoints. For speed-loop tuning, set the compile-time
`VOFA_SPEED_PID_TELEMETRY_ENABLE` switch in `main.c` from `0U` to `1U` and select
JustFloat in VOFA+. The lower-priority telemetry task sends one fixed 20-byte frame
every 10 ms. Its four float32 channels follow `g_motor_debug.wheel` and are ordered
as `target_speed_mm_per_s`, `instant_feedback_speed_mm_per_s`,
`feedback_speed_mm_per_s`, and `output_duty_percent`; the frame ends in
`00 00 80 7F`.

When VOFA mode is enabled, it owns UART0 output: the UART echo task is not
created. IMU yaw telemetry and speed-loop VOFA telemetry are compile-time
mutually exclusive. Do not send text to UART0 while VOFA mode is enabled. Frames are still queued through
`board_uart_write()` and emitted by the dedicated UART TX task, so the motor task
and encoder ISR never block on the UART. If `g_motor_debug.wheel` is invalid, the
telemetry task safely sends the front-left wheel status. For initial validation, use
the debugger without breakpoints to turn one wheel by hand and confirm count sign
and isolation before driving the chassis at low duty.

For grayscale observation, build with `-VofaSpeedPidTelemetryEnable 0
-GrayVofaTelemetryEnable 1` and select JustFloat at 115200 baud. The gray task
sends a 22-channel, 92-byte little-endian float32 frame every 100 ms:

| Channel | Value |
| --- | --- |
| 0..7 | `raw[0..7]` |
| 8..15 | `normalized[0..7]` |
| 16 | `digital` (`1=white`, `0=black`) |
| 17 | `black_mask` (`bit N` maps to `channel[N]`, `1=black`) |
| 18 | `black_count` |
| 19 | `line_error` |
| 20 | `line_strength` |
| 21 | `sequence` |

The frame tail is `00 00 80 7F`. Speed and grayscale VOFA telemetry are mutually
exclusive and enabling both is a compile-time error. In either VOFA mode UART
echo and BMI160 text output are suppressed. The grayscale fields are observation
only; this change does not modify motor targets or PWM output. For initial
validation, move a black line across the sensor and confirm that `normalized`,
`black_mask`, `line_error`, and `sequence` change together.

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
the Bosch reference example. IMU yaw telemetry, when enabled with
`IMU_YAW_ENABLE`, uses the existing static UART frame queue for JustFloat frames.
SPI transactions have bounded timeouts;
initialization retries after one second and three consecutive read failures
trigger reinitialization.

The implementation has passed the static integration check and TI Clang build.
Hardware validation confirmed `CHIP_ID=0xD1`, approximately 1g on stationary Z
acceleration, and near-zero stationary gyroscope output. No Flash write is
performed by the build and test commands.

`main.c` provides the compile-time `IMU_TELEMETRY_ENABLE` switch. It defaults
to `0U`; when enabled together with `IMU_YAW_ENABLE`, it sends one 16-byte
JustFloat frame every 10 ms through the existing UART frame queue. The three
float32 channels are `yaw_deg`, `yaw_rate_dps`, and `gyro_bias_z_dps`, followed
by the standard `00 00 80 7F` tail. BMI160 initialization and sampling continue
regardless of this output switch.

PA7, PB12, PA8, and PA30 are four external-pull-up, active-low button inputs.
The independent static `button_task` scans them every 10 ms and confirms a
state after two consecutive samples. Stable press and release edges are sent
through the serialized UART frame queue in pin order using lines such as
`key,pa7=down\r\n` and `key,pa7=up\r\n`. SysConfig leaves the internal resistor
disabled and does not enable GPIO interrupts for these inputs. The button task
uses a 128-word stack; the TI Clang map reports a 512-byte stack, 76-byte task
control block, and 12 bytes of button driver state. `main.c` provides the
compile-time `BUTTON_FEATURE_ENABLE` switch, defaulting to `0U`. Setting it to
`1U` enables button initialization, state scanning, UART reports, and the
button task's static RAM while keeping the SysConfig pin definitions unchanged.
## Eight-channel grayscale sensor

The Ganv no-MCU eight-channel sensor uses a 74HC4051 analog multiplexer. Wire
AD0 to PB13, AD1 to PB1, AD2 to PB23, and OUT to PA27. Leave EN floating; the
sensor's internal pulldown keeps it enabled. Connect sensor ground to MCU
ground and power the sensor from a stable separate 5 V supply.

`board_grayscale.c/.h` selects channels in address order: channel 0 is 000 and
channel 7 is 111. Every channel waits approximately 1 us after address change
and averages eight single 12-bit ADC conversions. The driver exposes raw and
0..4095 normalized arrays plus an eight-bit hysteresis result. `digital` uses
1 for white and 0 for black. The application derives `black_mask`, where bit N
corresponds to channel N and 1 means black, together with `black_count`,
`line_strength`, and the signed integer `line_error`.

The optional one-dimensional vehicle yaw estimator is implemented in
`board_imu_yaw.c/.h` and runs inside the existing 10 ms `imu_task`; it does not
create another FreeRTOS task. It converts the BMI160 Z gyro using the configured
`+/-500 dps` range, estimates the startup gyro bias during 100 stationary
samples, and fuses gyro yaw rate with the left/right differential encoder rate
using 98% gyro and 2% encoder weighting. The initial track width is configured
by `IMU_YAW_TRACK_WIDTH_MM` in `main.c`; the measured left/right wheel-center
distance is 130 mm. The default
`IMU_YAW_ENABLE` value is `0U`; set it to `1U` to enable the estimator.
The estimator assumes the vehicle frame is `+X` forward, `+Y` left, `+Z` up,
with positive Z gyro rate meaning a left turn. Change
`BOARD_IMU_YAW_GYRO_Z_SIGN` to `-1.0f` if the installed sensor has the opposite
Z direction.

When both `IMU_YAW_ENABLE` and `IMU_TELEMETRY_ENABLE` are `1U`, the IMU task
sends the three-channel JustFloat yaw frame described above. The yaw is
relative to the startup heading and is normalized to `[-180, 180)`; a six-axis
IMU cannot provide an absolute yaw reference without a magnetometer or another
external heading source.

The default `VOFA_SPEED_PID_TELEMETRY_ENABLE` value is `0U`. For the yaw UART
test, build with `-VofaSpeedPidTelemetryEnable 0 -ImuTelemetryEnable 1
-ImuYawEnable 1`; VOFA speed telemetry and IMU yaw telemetry cannot be enabled
together.

`main.c` uses the measured per-channel calibration values:

```text
white = {2834, 3064, 2150, 1924, 3099, 3032, 3182, 2467}
black = { 353, 1075,  139,  189, 1027,  593, 2033,  110}
```

`line_error` is calculated from normalized analog values using blackness
`4095-normalized[i]` and weights `{-3500,-2500,-1500,-500,500,1500,2500,3500}`.
When total blackness is zero it holds the previous error. It is currently an
observation input only; it does not change motor targets or PWM output.
`GRAY_VOFA_TELEMETRY_ENABLE` defaults to `0U`. Set it through the build script to
emit the binary frame described above. The volatile `g_grayscale_snapshot` is
available for SWD observation even when telemetry is disabled. Build success does
not constitute physical sensor acceptance.

## FreeRTOS CPU and task monitor

The optional `rtos_monitor` module is disabled by default and does not use
UART0. Pass `-RtosMonitorEnable 1` to the application or Keil PowerShell build
script to enable a static
monitor task that updates `g_rtos_monitor_snapshot` every 1000 ms for SWD
observation. The snapshot contains total CPU and idle percentages, task names,
states, priorities, runtime percentages, runtime in microseconds, and stack
high-water marks in `StackType_t` words. `sequence` is odd while a snapshot is
being published and even after the update is complete.

For manual configuration, edit `app_config.h` and set
`RTOS_MONITOR_ENABLE` to `1U`. `main.c` and `FreeRTOSConfig.h` both include this
shared header, so there is only one source-level switch. The build parameter is
still useful for automated or temporary builds and overrides the header default
without changing the file.

The runtime counter uses the unconnected `TIMG12` timer. The timer runs at
10 MHz from BUSCLK divided by 8; this is the highest stable free-running rate
available without changing the existing timer assignments. `timer_hz` in the
snapshot records the actual rate. Runtime time includes interrupt execution in
the task that was interrupted, so the first version does not report a separate
ISR percentage. The counter is 32-bit and the monitor uses unsigned deltas for
each sampling window. However, the bundled FreeRTOS kernel does not fully
protect its per-task cumulative runtime counters from timer wrap. At 10 MHz
the counter wraps after approximately 429 seconds, so long continuous runs
may make per-task values inaccurate after that point.
## CRSF remote control

The optional CRSF remote-control path uses UART3 at 420000 baud, 8-N-1, with
PB3 as RX and PB2 as TX. Connect the receiver TX output to PB3 and share MCU
ground. The firmware only receives CRSF data; it never uploads telemetry or
other frames to the receiver. The receiver output must be 3.3 V, non-inverted
UART TTL.

Build the remote-control variant with
`tools/build-mspm0g3507-app.ps1 -CrsfRemoteControlEnable 1`. The default build
keeps CRSF control disabled. CH3 (channel index 2) controls forward/reverse
and CH1 (channel index 0) controls differential steering. The standard CRSF
range 172..1811 is mapped around 992 with a 20 percent deadband. The default
maximum wheel target is 800 mm/s and can be changed with
`CRSF_MAX_SPEED_MM_PER_S`.

The four targets use left/right differential mixing and are normalized together
when the combined command exceeds the configured maximum. If no valid packed
RC frame arrives for 100 ms, all four targets are set to zero. Direction signs
can be adjusted with `CRSF_FORWARD_SIGN` and `CRSF_TURN_SIGN`. When the CRSF
compile-time switch is enabled, the SWD single-wheel debug override is compiled
out for that build; UART0 remains available for existing debug and VOFA output.

For SWD observation, expand the volatile `g_crsf_debug` structure. Its fields are
`channels.channels[0..15]`, `link_active`, `last_valid_time_ms`,
`valid_frame_count`, `crc_error_count`, `frame_error_count`, and
`rx_overflow_count`. `channels.channels[2]` is CH3 and
`channels.channels[0]` is CH1.

Host protocol and mixer tests are in `tests/test_crsf.ps1`. Hardware acceptance
must first be performed with the wheels lifted or the motor supply disconnected.
