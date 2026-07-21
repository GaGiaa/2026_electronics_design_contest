[CmdletBinding()]
param(
    [string] $ProjectRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

function Assert-Contains {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Pattern,
        [Parameter(Mandatory)] [string] $Description
    )

    if (-not (Select-String -Path $Path -Pattern $Pattern -Quiet)) {
        throw "${Description}: '$Pattern' was not found in $Path."
    }
}

function Assert-NotContains {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Pattern,
        [Parameter(Mandatory)] [string] $Description
    )

    if (Select-String -Path $Path -Pattern $Pattern -Quiet) {
        throw "${Description}: '$Pattern' must not be present in $Path."
    }
}

function Assert-OrderedPatterns {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string[]] $Patterns,
        [Parameter(Mandatory)] [string] $Description
    )

    $content = Get-Content -Raw $Path
    $offset = 0
    foreach ($pattern in $Patterns) {
        $match = [regex]::Match($content.Substring($offset), $pattern)
        if (-not $match.Success) {
            throw "${Description}: '$pattern' was not found in order in $Path."
        }
        $offset += $match.Index + $match.Length
    }
}

$projectDir = Join-Path $ProjectRoot 'mspm0g3507_app'
$main = Join-Path $projectDir 'main.c'
$ws2812 = Join-Path $projectDir 'board_ws2812.c'
$ws2812Header = Join-Path $projectDir 'board_ws2812.h'
$buzzer = Join-Path $projectDir 'board_buzzer.c'
$buzzerHeader = Join-Path $projectDir 'board_buzzer.h'
$uart = Join-Path $projectDir 'board_uart.c'
$motor = Join-Path $projectDir 'board_motor.c'
$motorHeader = Join-Path $projectDir 'board_motor.h'
$encoder = Join-Path $projectDir 'board_encoder.c'
$encoderHeader = Join-Path $projectDir 'board_encoder.h'
$syscfg = Join-Path $projectDir 'mspm0g3507_app.syscfg'
$rtosConfig = Join-Path $projectDir 'FreeRTOSConfig.h'
$ccsBuildConfig = Join-Path $projectDir '.cproject'
$buildScript = Join-Path $ProjectRoot 'tools\build-mspm0g3507-app.ps1'
$flashScript = Join-Path $ProjectRoot 'tools\flash-and-debug-g3507.ps1'
$debugStartScript = Join-Path $ProjectRoot 'tools\start-mspm0g3507-app-debug.ps1'
$debugStopScript = Join-Path $ProjectRoot 'tools\stop-mspm0g3507-app-debug.ps1'
$tasks = Join-Path $ProjectRoot '.vscode\tasks.json'
$launch = Join-Path $ProjectRoot '.vscode\launch.json'
$gitIgnore = Join-Path $ProjectRoot '.gitignore'

foreach ($path in @($projectDir, $main, $ws2812, $ws2812Header, $buzzer, $buzzerHeader, $uart, $motor, $motorHeader, $encoder, $encoderHeader, $syscfg, $rtosConfig, $ccsBuildConfig,
                         $buildScript, $flashScript, $debugStartScript, $debugStopScript, $tasks, $launch, $gitIgnore)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "MSPM0G3507 app project is incomplete: $path is missing."
    }
}

Assert-Contains -Path $main -Pattern 'xTaskCreateStatic' -Description 'WS2812 and UART tasks must use static allocation'
Assert-Contains -Path $main -Pattern 'xQueueCreateStatic' -Description 'UART queue must use static allocation'
Assert-Contains -Path $main -Pattern 'pdMS_TO_TICKS\(500U\)' -Description 'WS2812 animation must update every 500 ms'
Assert-Contains -Path $main -Pattern 'WS2812_BRIGHTNESS\s+\d+U' -Description 'WS2812 animation must define a channel brightness'
Assert-Contains -Path $main -Pattern 'board_ws2812_write' -Description 'WS2812 animation must submit frames'
Assert-Contains -Path $main -Pattern 'MOTOR_FRONT_LEFT_DIRECTION' -Description 'Application must define a front-left motor direction macro'
Assert-Contains -Path $main -Pattern 'MOTOR_FRONT_RIGHT_DIRECTION' -Description 'Application must define a front-right motor direction macro'
Assert-Contains -Path $main -Pattern 'MOTOR_REAR_LEFT_DIRECTION' -Description 'Application must define a rear-left motor direction macro'
Assert-Contains -Path $main -Pattern 'MOTOR_REAR_RIGHT_DIRECTION' -Description 'Application must define a rear-right motor direction macro'
Assert-Contains -Path $main -Pattern 'pdMS_TO_TICKS\(10U\)' -Description 'Motor task must refresh commands every 10 ms'
Assert-Contains -Path $main -Pattern 'xTaskCreateStatic\(motor_task' -Description 'Motor task must use static allocation'
Assert-Contains -Path $main -Pattern 'board_encoder_init' -Description 'Application must initialize the encoder driver'
Assert-OrderedPatterns -Path $main -Patterns @('board_encoder_sample', 'board_motor_set') -Description 'Motor task must sample encoders before updating PWM'
Assert-Contains -Path $main -Pattern 'GROUP1_IRQHandler' -Description 'Application must provide the GPIOA interrupt handler for encoder A phases'
Assert-Contains -Path $main -Pattern 'g_encoder_samples' -Description 'Application must retain encoder samples for debugger observation'
Assert-Contains -Path $main -Pattern 'ENCODER_TELEMETRY_INTERVAL_MS\s+100U' -Description 'Encoder telemetry must run at 10 Hz'
Assert-Contains -Path $main -Pattern 'ENCODER_TELEMETRY_TASK_STACK_DEPTH\s+512U' -Description 'Telemetry task must have enough stack for formatted encoder reports'
Assert-Contains -Path $main -Pattern 'telemetry_task' -Description 'Application must provide a telemetry task'
Assert-Contains -Path $main -Pattern 'xTaskCreateStatic\(telemetry_task' -Description 'Encoder telemetry task must use static allocation'
Assert-Contains -Path $main -Pattern 'board_uart_write' -Description 'Telemetry must send through the serialized UART writer'
Assert-Contains -Path $main -Pattern 'xTaskCreateStatic\(board_uart_tx_task' -Description 'Application must create the dedicated UART transmit task'
Assert-NotContains -Path $main -Pattern 'continue;' -Description 'Telemetry snapshot retries must not compare an uninitialized sequence value'
Assert-Contains -Path $main -Pattern 'BUZZER_FEATURE_ENABLE\s+0U' -Description 'Buzzer feature must default to disabled'
Assert-Contains -Path $main -Pattern 'BUZZER_FREQUENCY_HZ\s+2000U' -Description 'Buzzer frequency must default to 2 kHz'
Assert-Contains -Path $main -Pattern 'BUZZER_DUTY_PERCENT\s+50U' -Description 'Buzzer duty cycle must default to 50 percent'
Assert-Contains -Path $main -Pattern 'BUZZER_ON_TIME_MS\s+200U' -Description 'Buzzer on time must default to 200 ms'
Assert-Contains -Path $main -Pattern 'BUZZER_OFF_TIME_MS\s+1800U' -Description 'Buzzer off time must default to 1800 ms'
Assert-Contains -Path $main -Pattern '#if BUZZER_FEATURE_ENABLE' -Description 'Buzzer task creation must be compile-time gated'
Assert-Contains -Path $main -Pattern 'xTaskCreateStatic\(buzzer_task' -Description 'Buzzer task must use static allocation'
Assert-Contains -Path $main -Pattern 'pdMS_TO_TICKS\(BUZZER_ON_TIME_MS\)' -Description 'Buzzer task must use configured on time'
Assert-Contains -Path $main -Pattern 'pdMS_TO_TICKS\(BUZZER_OFF_TIME_MS\)' -Description 'Buzzer task must use configured off time'
Assert-NotContains -Path $main -Pattern 'SysTick_Handler' -Description 'Application must not replace the FreeRTOS tick handler'
Assert-Contains -Path $buzzerHeader -Pattern 'board_buzzer_init' -Description 'Buzzer driver must expose initialization'
Assert-Contains -Path $buzzerHeader -Pattern 'board_buzzer_start' -Description 'Buzzer driver must expose start'
Assert-Contains -Path $buzzerHeader -Pattern 'board_buzzer_stop' -Description 'Buzzer driver must expose stop'
Assert-Contains -Path $buzzer -Pattern 'DL_Timer_setLoadValue' -Description 'Buzzer driver must set the PWM period'
Assert-Contains -Path $buzzer -Pattern 'DL_Timer_setCaptureCompareValue' -Description 'Buzzer driver must set the PWM duty cycle'
Assert-Contains -Path $buzzer -Pattern 'BUZZER_INST_CLK_FREQ' -Description 'Buzzer driver must use the generated timer clock frequency'
Assert-Contains -Path $buzzer -Pattern 'duty_percent > 100U' -Description 'Buzzer driver must clamp duty cycles above 100 percent'
Assert-Contains -Path $buzzer -Pattern 'frequency_hz < 1000U' -Description 'Buzzer driver must reject frequencies below 1 kHz'
Assert-Contains -Path $buzzer -Pattern 'frequency_hz > 20000U' -Description 'Buzzer driver must reject frequencies above 20 kHz'
Assert-Contains -Path $buzzer -Pattern 'BUZZER_INST' -Description 'Buzzer driver must use the generated PWM instance'
Assert-Contains -Path $motorHeader -Pattern 'BOARD_MOTOR_DIRECTION_STOP' -Description 'Motor driver must expose a stop direction'
Assert-Contains -Path $motorHeader -Pattern 'BOARD_MOTOR_DIRECTION_FORWARD' -Description 'Motor driver must expose a forward direction'
Assert-Contains -Path $motorHeader -Pattern 'BOARD_MOTOR_DIRECTION_REVERSE' -Description 'Motor driver must expose a reverse direction'
Assert-Contains -Path $motor -Pattern 'duty_percent > 100U' -Description 'Motor driver must clamp duty cycles above 100 percent'
Assert-Contains -Path $motor -Pattern 'DL_Timer_setCaptureCompareValue' -Description 'Motor driver must update PWM compare values'
Assert-Contains -Path $motor -Pattern 'BOARD_MOTOR_DIRECTION_FORWARD' -Description 'Motor driver must drive IN1 for forward motion'
Assert-Contains -Path $motor -Pattern 'BOARD_MOTOR_DIRECTION_REVERSE' -Description 'Motor driver must drive IN2 for reverse motion'
Assert-OrderedPatterns -Path $motor -Patterns @('case BOARD_MOTOR_FRONT_LEFT:', 'MOTOR_FRONT_LEFT_INST') -Description 'Logical front-left wheel must use its matching PWM instance'
Assert-OrderedPatterns -Path $motor -Patterns @('case BOARD_MOTOR_FRONT_RIGHT:', 'MOTOR_FRONT_RIGHT_INST') -Description 'Logical front-right wheel must use its matching PWM instance'
Assert-OrderedPatterns -Path $motor -Patterns @('case BOARD_MOTOR_REAR_LEFT:', 'BOARD_MOTOR_DIRECTION_REVERSE', 'MOTOR_REAR_LEFT_INST') -Description 'Logical rear-left wheel must invert direction through its matching PWM instance'
Assert-OrderedPatterns -Path $motor -Patterns @('case BOARD_MOTOR_REAR_RIGHT:', 'MOTOR_REAR_RIGHT_INST') -Description 'Logical rear-right wheel must use its matching PWM instance'
Assert-Contains -Path $uart -Pattern 'xQueueSendFromISR' -Description 'UART ISR must queue received bytes'
Assert-Contains -Path $uart -Pattern 'DL_UART_Main_isRXFIFOEmpty' -Description 'UART ISR must drain the receive FIFO'
Assert-Contains -Path $uart -Pattern 'NVIC_SetPriority' -Description 'UART ISR priority must be compatible with FreeRTOS FromISR APIs'
Assert-Contains -Path $uart -Pattern 'board_uart_write' -Description 'UART driver must provide a frame write interface'
Assert-Contains -Path $uart -Pattern 'xQueueCreateStatic' -Description 'UART writer must use a static transmit queue'
Assert-Contains -Path $uart -Pattern 'xQueueSend' -Description 'UART frame writes must enqueue whole messages'
Assert-Contains -Path $uart -Pattern 'xQueueReceive' -Description 'Dedicated UART transmit task must dequeue messages'
Assert-Contains -Path $uart -Pattern 'board_uart_tx_task' -Description 'UART driver must provide the dedicated transmit task'
Assert-Contains -Path $encoderHeader -Pattern 'board_encoder_sample_t' -Description 'Encoder driver must expose a sample type'
Assert-Contains -Path $encoderHeader -Pattern 'delta_counts' -Description 'Encoder samples must expose interval counts'
Assert-Contains -Path $encoderHeader -Pattern 'total_counts' -Description 'Encoder samples must expose accumulated counts'
Assert-Contains -Path $encoderHeader -Pattern 'speed_mm_per_s' -Description 'Encoder samples must expose wheel speed'
Assert-Contains -Path $encoderHeader -Pattern 'BOARD_ENCODER_COUNTS_PER_REVOLUTION\s+1040' -Description 'Encoder driver must use two A-phase edges per 520-count revolution by default'
Assert-Contains -Path $encoderHeader -Pattern 'BOARD_ENCODER_WHEEL_DIAMETER_MM\s+48' -Description 'Encoder driver must define the reference wheel diameter'
Assert-Contains -Path $encoder -Pattern 'DL_GPIO_getPendingInterrupt\(GPIOA\)' -Description 'Encoder driver must dispatch GPIOA edge interrupts'
Assert-Contains -Path $encoder -Pattern 'DL_GPIO_readPins' -Description 'Encoder driver must read B phases to determine direction'
Assert-Contains -Path $encoder -Pattern 'RISE_FALL' -Description 'Encoder source must document A-phase dual-edge handling'
Assert-Contains -Path $encoder -Pattern 'ENCODER_FRONT_LEFT_A_IIDX' -Description 'Encoder driver must use generated front-left A interrupt definitions'
Assert-Contains -Path $encoder -Pattern 'ENCODER_FRONT_RIGHT_A_IIDX' -Description 'Encoder driver must use generated front-right A interrupt definitions'
Assert-Contains -Path $encoder -Pattern 'ENCODER_REAR_LEFT_A_IIDX' -Description 'Encoder driver must use generated rear-left A interrupt definitions'
Assert-Contains -Path $encoder -Pattern 'ENCODER_REAR_RIGHT_A_IIDX' -Description 'Encoder driver must use generated rear-right A interrupt definitions'
Assert-OrderedPatterns -Path $encoder -Patterns @(
    'case ENCODER_FRONT_LEFT_A_IIDX:',
    'BOARD_MOTOR_FRONT_LEFT',
    '\s*,\s*1\);',
    'case ENCODER_FRONT_RIGHT_A_IIDX:',
    'BOARD_MOTOR_FRONT_RIGHT',
    '\s*,\s*-1\);',
    'case ENCODER_REAR_LEFT_A_IIDX:',
    'BOARD_MOTOR_REAR_LEFT',
    '\s*,\s*1\);',
    'case ENCODER_REAR_RIGHT_A_IIDX:',
    'BOARD_MOTOR_REAR_RIGHT',
    '\s*,\s*-1\);'
) -Description 'Encoder GPIO channels must match the measured physical wheel mapping and direction signs'
Assert-Contains -Path $syscfg -Pattern 'PB22' -Description 'WS2812 data output must use PB22'
Assert-Contains -Path $syscfg -Pattern 'HSCLKMUX.*SYSPLL0' -Description 'Application CPU clock must use SYSPLL0'
Assert-Contains -Path $syscfg -Pattern 'HFXT' -Description 'Application SYSPLL reference must use the HFXT'
Assert-Contains -Path $syscfg -Pattern 'inputFreq\s*=\s*40' -Description 'Application HFXT frequency must be 40 MHz'
Assert-Contains -Path $syscfg -Pattern 'pllQDiv\.multiplyValue\s*=\s*5' -Description 'Application SYSPLL multiplier must produce an 80 MHz CPU clock'
Assert-Contains -Path $syscfg -Pattern 'ulpDiv\.divideValue\s*=\s*2' -Description 'Application ULPCLK must divide the 80 MHz CPU clock by two'
Assert-Contains -Path $rtosConfig -Pattern 'configCPU_CLOCK_HZ\s+\(80000000UL\)' -Description 'FreeRTOS CPU clock must match the 80 MHz SysConfig clock'
Assert-Contains -Path $syscfg -Pattern 'PA10' -Description 'UART0 TX must remain on PA10'
Assert-Contains -Path $syscfg -Pattern 'PA11' -Description 'UART0 RX must remain on PA11'
Assert-Contains -Path $syscfg -Pattern 'enableFIFO\s*=\s*true' -Description 'UART0 receive FIFO must be enabled'
Assert-Contains -Path $syscfg -Pattern 'ENCODER' -Description 'SysConfig must define the encoder input group'
Assert-Contains -Path $syscfg -Pattern 'PA16' -Description 'Front-left encoder A phase must use PA16'
Assert-Contains -Path $syscfg -Pattern 'PB20' -Description 'Front-left encoder B phase must use PB20'
Assert-Contains -Path $syscfg -Pattern 'PA14' -Description 'Front-right encoder A phase must use PA14'
Assert-Contains -Path $syscfg -Pattern 'PA9' -Description 'Front-right encoder B phase must use PA9'
Assert-Contains -Path $syscfg -Pattern 'PA15' -Description 'Rear-left encoder A phase must use PA15'
Assert-Contains -Path $syscfg -Pattern 'PB24' -Description 'Rear-left encoder B phase must use PB24'
Assert-Contains -Path $syscfg -Pattern 'PA17' -Description 'Rear-right encoder A phase must use PA17'
Assert-Contains -Path $syscfg -Pattern 'PA22' -Description 'Rear-right encoder B phase must use PA22'
Assert-Contains -Path $syscfg -Pattern 'RISE_FALL' -Description 'Encoder A phases must interrupt on both edges'
Assert-Contains -Path $syscfg -Pattern 'PULL_UP' -Description 'Encoder inputs must use pull-ups'
Assert-OrderedPatterns -Path $syscfg -Patterns @('FRONT_LEFT_A', 'interruptEn\s*=\s*true', 'polarity\s*=\s*"RISE_FALL"', 'PA15') -Description 'Front-left A must use PA15 dual-edge interrupts'
Assert-OrderedPatterns -Path $syscfg -Patterns @('FRONT_LEFT_B', 'PULL_UP', 'PB24') -Description 'Front-left B must use PB24 pull-up input'
Assert-OrderedPatterns -Path $syscfg -Patterns @('FRONT_RIGHT_A', 'interruptEn\s*=\s*true', 'polarity\s*=\s*"RISE_FALL"', 'PA17') -Description 'Front-right A must use PA17 dual-edge interrupts'
Assert-OrderedPatterns -Path $syscfg -Patterns @('FRONT_RIGHT_B', 'PULL_UP', 'PA22') -Description 'Front-right B must use PA22 pull-up input'
Assert-OrderedPatterns -Path $syscfg -Patterns @('REAR_LEFT_A', 'interruptEn\s*=\s*true', 'polarity\s*=\s*"RISE_FALL"', 'PA14') -Description 'Rear-left A must use PA14 dual-edge interrupts'
Assert-OrderedPatterns -Path $syscfg -Patterns @('REAR_LEFT_B', 'PULL_UP', 'PA9') -Description 'Rear-left B must use PA9 pull-up input'
Assert-OrderedPatterns -Path $syscfg -Patterns @('REAR_RIGHT_A', 'interruptEn\s*=\s*true', 'polarity\s*=\s*"RISE_FALL"', 'PA16') -Description 'Rear-right A must use PA16 dual-edge interrupts'
Assert-OrderedPatterns -Path $syscfg -Patterns @('REAR_RIGHT_B', 'PULL_UP', 'PB20') -Description 'Rear-right B must use PB20 pull-up input'
Assert-Contains -Path $syscfg -Pattern 'MOTOR_FRONT_LEFT' -Description 'SysConfig must define the front-left motor PWM'
Assert-Contains -Path $syscfg -Pattern 'MOTOR_FRONT_RIGHT' -Description 'SysConfig must define the front-right motor PWM'
Assert-Contains -Path $syscfg -Pattern 'MOTOR_REAR_LEFT' -Description 'SysConfig must define the rear-left motor PWM'
Assert-Contains -Path $syscfg -Pattern 'MOTOR_REAR_RIGHT' -Description 'SysConfig must define the rear-right motor PWM'
Assert-Contains -Path $syscfg -Pattern 'PA12' -Description 'Front-left motor IN1 must use PA12'
Assert-Contains -Path $syscfg -Pattern 'PA13' -Description 'Front-left motor IN2 must use PA13'
Assert-Contains -Path $syscfg -Pattern 'PA28' -Description 'Front-right motor IN1 must use PA28'
Assert-Contains -Path $syscfg -Pattern 'PA31' -Description 'Front-right motor IN2 must use PA31'
Assert-Contains -Path $syscfg -Pattern 'PA29' -Description 'Rear-left motor IN1 must use PA29'
Assert-Contains -Path $syscfg -Pattern 'PB27' -Description 'Rear-left motor IN2 must use PB27'
Assert-Contains -Path $syscfg -Pattern 'PB4' -Description 'Rear-right motor IN1 must use PB4'
Assert-Contains -Path $syscfg -Pattern 'PB5' -Description 'Rear-right motor IN2 must use PB5'
Assert-Contains -Path $syscfg -Pattern 'timerCount\s*=\s*4000' -Description 'Motor PWM must use a 4000-tick period at its 40 MHz timer clock'
Assert-Contains -Path $syscfg -Pattern 'PWMFrontLeft\.clockPrescale\s*=\s*2' -Description 'Front-left motor timer must prescale 80 MHz to 40 MHz'
Assert-Contains -Path $syscfg -Pattern 'PWMFrontRight\.clockDivider\s*=\s*2' -Description 'Front-right motor timer must divide 80 MHz to 40 MHz'
Assert-Contains -Path $syscfg -Pattern 'PWMRearLeft\.clockDivider\s*=\s*2' -Description 'Rear-left motor timer must divide 80 MHz to 40 MHz'
Assert-Contains -Path $syscfg -Pattern 'BUZZER' -Description 'SysConfig must define the buzzer PWM'
Assert-Contains -Path $syscfg -Pattern 'TIMG8' -Description 'Buzzer must use the dedicated TIMG8 peripheral'
Assert-Contains -Path $syscfg -Pattern 'ccp1Pin\.\$assign\s*=\s*"PA2"' -Description 'Buzzer PWM output must use PA2 CCP1'
Assert-Contains -Path $syscfg -Pattern 'PWMBuzzer\.timerCount\s*=\s*20000' -Description 'Buzzer timer must use the 2 kHz default period'
Assert-Contains -Path $ws2812Header -Pattern 'BOARD_WS2812_PIXEL_COUNT\s+4U' -Description 'WS2812 driver must target exactly four pixels'
Assert-Contains -Path $ws2812 -Pattern 'taskDISABLE_INTERRUPTS' -Description 'WS2812 frame writes must mask interrupts'
Assert-Contains -Path $ws2812 -Pattern 'taskENABLE_INTERRUPTS' -Description 'WS2812 frame writes must restore interrupts'
Assert-Contains -Path $ws2812 -Pattern 'WS2812_SPI_FRAME_BYTES\s+60U' -Description 'WS2812 SPI frame must include data and reset-low bytes'
Assert-Contains -Path $ws2812 -Pattern 'DL_SPI_transmitData8' -Description 'WS2812 driver must transmit encoded bytes through SPI'
Assert-Contains -Path $ws2812 -Pattern 'DL_SPI_isBusy' -Description 'WS2812 driver must wait for the SPI shifter to finish'
Assert-Contains -Path $ws2812 -Pattern '0x04U' -Description 'WS2812 zero bits must encode as SPI 100'
Assert-Contains -Path $ws2812 -Pattern '0x06U' -Description 'WS2812 one bits must encode as SPI 110'
Assert-OrderedPatterns -Path $ws2812 -Patterns @('send_byte\(pixel->green,', 'send_byte\(pixel->red,', 'send_byte\(pixel->blue,') -Description 'WS2812 bytes must be sent in GRB order'
Assert-Contains -Path $syscfg -Pattern 'SPI1\.targetBitRate\s*=\s*2666667' -Description 'SPI1 must clock WS2812 symbols at 2.666667 MHz'
Assert-Contains -Path $syscfg -Pattern 'SPI1\.peripheral\.mosiPin\.\$assign\s*=\s*"PB22"' -Description 'SPI1 PICO must use PB22'
Assert-Contains -Path $buildScript -Pattern 'mspm0g3507_app\.out' -Description 'Build must link the app ELF'
Assert-Contains -Path $buildScript -Pattern 'board_ws2812\.c' -Description 'Build must compile the WS2812 driver'
Assert-Contains -Path $buildScript -Pattern 'board_motor\.c' -Description 'Build must compile the motor driver'
Assert-Contains -Path $buildScript -Pattern 'board_encoder\.c' -Description 'Build must compile the encoder driver'
Assert-Contains -Path $buildScript -Pattern 'board_buzzer\.c' -Description 'Build must compile the buzzer driver'
Assert-Contains -Path $flashScript -Pattern "'app'" -Description 'Flash script must select the app ELF explicitly'
Assert-Contains -Path $debugStartScript -Pattern "'-m', 'pyocd', 'gdbserver'" -Description 'App debug task must use the Python pyOCD module entry point'
Assert-Contains -Path $debugStartScript -Pattern 'ProbeUid\s*=\s*''2dd0719d''' -Description 'App debug task must select the calibrated DAPLink probe'
Assert-Contains -Path $debugStartScript -Pattern "'--frequency', '1000000'" -Description 'App debug task must use a stable 1 MHz SWD clock'
Assert-Contains -Path $debugStopScript -Pattern "'monitor exit'" -Description 'App debug cleanup must request a graceful pyOCD shutdown'
Assert-Contains -Path $tasks -Pattern 'MSPM0G3507 App: Build' -Description 'VS Code must provide an app build task'
Assert-Contains -Path $tasks -Pattern 'MSPM0G3507 App: Prepare debug' -Description 'VS Code must prepare the app build and GDB server automatically'
Assert-Contains -Path $tasks -Pattern 'GDB server listening on port 3333' -Description 'VS Code must wait for the GDB server readiness event'
Assert-Contains -Path $launch -Pattern 'MSPM0G3507 App: Attach to pyOCD' -Description 'VS Code must provide an app attach configuration'
Assert-Contains -Path $launch -Pattern '"gdbTarget": "localhost:3333"' -Description 'App debug must attach to the automatically managed GDB server'
Assert-Contains -Path $gitIgnore -Pattern '/mspm0g3507_app/Debug/' -Description 'App Debug output must be ignored'

Write-Host 'PASS: MSPM0G3507 app static integration checks passed.'
