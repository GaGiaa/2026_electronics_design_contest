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
