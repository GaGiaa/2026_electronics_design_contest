# MSPM0G3507 FreeRTOS bring-up

This is an independent G3507 CCS project. It uses PB22 for the user LED and
UART0 on PA10/PA11 for 115200 8-N-1 interrupt-driven echo.

The application uses a 1 kHz FreeRTOS tick, two statically allocated tasks,
and one statically allocated UART receive queue. It does not use FreeRTOS
dynamic object allocation.

Import the folder with `Project -> Import CCS Projects`. Use the supplied
PowerShell build task for the reproducible FreeRTOS build, because it compiles
the SDK FreeRTOS sources with this project's static-allocation configuration.
The output is `mspm0g3507_freertos/Debug/mspm0g3507_freertos.out`.
