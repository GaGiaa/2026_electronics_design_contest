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
$uart = Join-Path $projectDir 'board_uart.c'
$motor = Join-Path $projectDir 'board_motor.c'
$motorHeader = Join-Path $projectDir 'board_motor.h'
$syscfg = Join-Path $projectDir 'mspm0g3507_app.syscfg'
$rtosConfig = Join-Path $projectDir 'FreeRTOSConfig.h'
$ccsBuildConfig = Join-Path $projectDir '.cproject'
$buildScript = Join-Path $ProjectRoot 'tools\build-mspm0g3507-app.ps1'
$flashScript = Join-Path $ProjectRoot 'tools\flash-and-debug-g3507.ps1'
$tasks = Join-Path $ProjectRoot '.vscode\tasks.json'
$launch = Join-Path $ProjectRoot '.vscode\launch.json'
$gitIgnore = Join-Path $ProjectRoot '.gitignore'

foreach ($path in @($projectDir, $main, $ws2812, $ws2812Header, $uart, $motor, $motorHeader, $syscfg, $rtosConfig, $ccsBuildConfig,
                        $buildScript, $flashScript, $tasks, $launch, $gitIgnore)) {
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
Assert-NotContains -Path $main -Pattern 'SysTick_Handler' -Description 'Application must not replace the FreeRTOS tick handler'
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
Assert-Contains -Path $flashScript -Pattern "'app'" -Description 'Flash script must select the app ELF explicitly'
Assert-Contains -Path $tasks -Pattern 'MSPM0G3507 App: Build' -Description 'VS Code must provide an app build task'
Assert-Contains -Path $launch -Pattern 'MSPM0G3507 App: Attach to pyOCD' -Description 'VS Code must provide an app attach configuration'
Assert-Contains -Path $gitIgnore -Pattern '/mspm0g3507_app/Debug/' -Description 'App Debug output must be ignored'

Write-Host 'PASS: MSPM0G3507 app static integration checks passed.'
