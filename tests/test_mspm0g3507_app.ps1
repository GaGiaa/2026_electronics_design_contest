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
$syscfg = Join-Path $projectDir 'mspm0g3507_app.syscfg'
$rtosConfig = Join-Path $projectDir 'FreeRTOSConfig.h'
$ccsBuildConfig = Join-Path $projectDir '.cproject'
$buildScript = Join-Path $ProjectRoot 'tools\build-mspm0g3507-app.ps1'
$flashScript = Join-Path $ProjectRoot 'tools\flash-and-debug-g3507.ps1'
$tasks = Join-Path $ProjectRoot '.vscode\tasks.json'
$launch = Join-Path $ProjectRoot '.vscode\launch.json'
$gitIgnore = Join-Path $ProjectRoot '.gitignore'

foreach ($path in @($projectDir, $main, $ws2812, $ws2812Header, $uart, $syscfg, $rtosConfig, $ccsBuildConfig,
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
Assert-NotContains -Path $main -Pattern 'SysTick_Handler' -Description 'Application must not replace the FreeRTOS tick handler'
Assert-Contains -Path $uart -Pattern 'xQueueSendFromISR' -Description 'UART ISR must queue received bytes'
Assert-Contains -Path $uart -Pattern 'DL_UART_Main_isRXFIFOEmpty' -Description 'UART ISR must drain the receive FIFO'
Assert-Contains -Path $uart -Pattern 'NVIC_SetPriority' -Description 'UART ISR priority must be compatible with FreeRTOS FromISR APIs'
Assert-Contains -Path $syscfg -Pattern 'PB22' -Description 'WS2812 data output must use PB22'
Assert-Contains -Path $syscfg -Pattern 'PA10' -Description 'UART0 TX must remain on PA10'
Assert-Contains -Path $syscfg -Pattern 'PA11' -Description 'UART0 RX must remain on PA11'
Assert-Contains -Path $syscfg -Pattern 'enableFIFO\s*=\s*true' -Description 'UART0 receive FIFO must be enabled'
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
Assert-Contains -Path $flashScript -Pattern "'app'" -Description 'Flash script must select the app ELF explicitly'
Assert-Contains -Path $tasks -Pattern 'MSPM0G3507 App: Build' -Description 'VS Code must provide an app build task'
Assert-Contains -Path $launch -Pattern 'MSPM0G3507 App: Attach to pyOCD' -Description 'VS Code must provide an app attach configuration'
Assert-Contains -Path $gitIgnore -Pattern '/mspm0g3507_app/Debug/' -Description 'App Debug output must be ignored'

Write-Host 'PASS: MSPM0G3507 app static integration checks passed.'
