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

$projectDir = Join-Path $ProjectRoot 'mspm0g3507_freertos'
$main = Join-Path $projectDir 'main.c'
$uart = Join-Path $projectDir 'board_uart.c'
$syscfg = Join-Path $projectDir 'mspm0g3507_freertos.syscfg'
$rtosConfig = Join-Path $projectDir 'FreeRTOSConfig.h'
$buildScript = Join-Path $ProjectRoot 'tools\build-mspm0g3507-freertos.ps1'

foreach ($path in @($projectDir, $main, $uart, $syscfg, $rtosConfig, $buildScript)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "FreeRTOS G3507 project is incomplete: $path is missing."
    }
}

Assert-Contains -Path $main -Pattern 'xTaskCreateStatic' -Description 'LED and UART tasks must use static allocation'
Assert-Contains -Path $main -Pattern 'xQueueCreateStatic' -Description 'UART queue must use static allocation'
Assert-Contains -Path $main -Pattern 'vTaskDelayUntil' -Description 'LED task must use RTOS periodic delay'
Assert-NotContains -Path $main -Pattern 'SysTick_Handler' -Description 'Application must not replace the FreeRTOS tick handler'
Assert-Contains -Path $uart -Pattern 'xQueueSendFromISR' -Description 'UART ISR must send received data to a FreeRTOS queue'
Assert-Contains -Path $uart -Pattern 'NVIC_SetPriority' -Description 'UART ISR priority must be compatible with FreeRTOS FromISR APIs'
Assert-Contains -Path $syscfg -Pattern 'PB22' -Description 'User LED must remain on PB22'
Assert-Contains -Path $syscfg -Pattern 'PA10' -Description 'UART0 TX must remain on PA10'
Assert-Contains -Path $syscfg -Pattern 'PA11' -Description 'UART0 RX must remain on PA11'
Assert-Contains -Path $buildScript -Pattern 'FreeRTOSConfig.h' -Description 'Build must include the local FreeRTOS configuration'
Assert-Contains -Path $rtosConfig -Pattern 'configSUPPORT_STATIC_ALLOCATION\s+1' -Description 'FreeRTOS static allocation must be enabled'
Assert-Contains -Path $rtosConfig -Pattern 'configSUPPORT_DYNAMIC_ALLOCATION\s+0' -Description 'FreeRTOS dynamic allocation must be disabled'

Write-Host 'PASS: MSPM0G3507 FreeRTOS project static integration checks passed.'
