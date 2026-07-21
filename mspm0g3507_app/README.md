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
Four 10 kHz, two-input PWM motor channels use PA12/PA13 (front-left),
PA28/PA31 (front-right), PA29/PB27 (rear-left), and PB4/PB5 (rear-right).
`main.c` contains separate direction and duty-percent macros for every wheel;
all default to stop and 0 percent. The static motor task applies the macros
every 10 ms. Forward drives IN1 with PWM and holds IN2 low; reverse does the
opposite. The motor supply, driver and MCU must share ground. Hardware
acceptance starts with one wheel at a low duty cycle and requires explicit
authorization before any Flash write.
