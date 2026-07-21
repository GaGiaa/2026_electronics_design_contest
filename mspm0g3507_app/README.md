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
`main.c` contains separate direction and duty-percent macros for every wheel;
all default to stop and 0 percent. The static motor task applies the macros
every 10 ms. Forward drives IN1 with PWM and holds IN2 low; reverse does the
opposite. The motor supply, driver and MCU must share ground. Hardware
acceptance starts with one wheel at a low duty cycle and requires explicit
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
and calculated mm/s speed before updating PWM. The current PWM commands remain open
loop.

For observation, `g_encoder_samples[BOARD_MOTOR_COUNT]` is a volatile global
snapshot written by the 10 ms motor task and can be watched through SWD without
adding breakpoints. A lower-priority telemetry task also transmits one line every
100 ms through UART0. Each wheel is reported as `delta_counts,total_counts,speed`:

```text
enc,fl=12,1240,181,fr=11,1228,165,rl=12,1237,181,rr=11,1221,165
```

Speed is truncated to integer mm/s in the serial frame. UART echo and telemetry
enqueue whole messages to a static frame queue, and one transmit task owns the
hardware FIFO so bytes from different messages cannot interleave. Telemetry does
not run in the encoder ISR or motor task. For initial validation,
use the debugger without breakpoints to turn one wheel by hand and confirm count
sign and isolation before driving the chassis at low duty.

PA2 drives a passive buzzer through TIMG8 CCP1. `main.c` provides the
compile-time `BUZZER_FEATURE_ENABLE`, `BUZZER_FREQUENCY_HZ`,
`BUZZER_DUTY_PERCENT`, `BUZZER_ON_TIME_MS`, and `BUZZER_OFF_TIME_MS` macros.
The defaults are disabled, 2000 Hz, 50 percent, 200 ms on, and 1800 ms off.
When enabled, a dedicated static FreeRTOS task repeats the on/off interval;
when disabled, the PWM is initialized with a zero compare value and no buzzer
task is created. The passive buzzer driver circuit and MCU must share ground.
