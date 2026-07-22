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

    if (-not (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) {
        throw "${Description}: '$Pattern' was not found in $Path."
    }
}

function Assert-NotContains {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Pattern,
        [Parameter(Mandatory)] [string] $Description
    )

    if (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet) {
        throw "${Description}: '$Pattern' must not be present in $Path."
    }
}

$projectDir = Join-Path $ProjectRoot 'mspm0g3507_app'
$header = Join-Path $projectDir 'rtos_monitor.h'
$source = Join-Path $projectDir 'rtos_monitor.c'
$appConfig = Join-Path $projectDir 'app_config.h'
$config = Join-Path $projectDir 'FreeRTOSConfig.h'
$main = Join-Path $projectDir 'main.c'
$syscfg = Join-Path $projectDir 'mspm0g3507_app.syscfg'
$ccsBuild = Join-Path $ProjectRoot 'tools\build-mspm0g3507-app.ps1'
$keilBuild = Join-Path $ProjectRoot 'tools\build-keil-mspm0g3507-app.ps1'
$keilProject = Join-Path $ProjectRoot 'keil\mspm0g3507_app\mspm0g3507_app.uvprojx'

foreach ($path in @($header, $source, $appConfig, $config, $main, $syscfg, $ccsBuild, $keilBuild, $keilProject)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required RTOS monitor file is missing: $path"
    }
}

Assert-Contains -Path $header -Pattern 'g_rtos_monitor_snapshot' -Description 'Monitor header must export the SWD snapshot'
Assert-Contains -Path $header -Pattern 'RTOS_MONITOR_MAX_TASKS\s+16U' -Description 'Monitor must reserve 16 task slots'
Assert-Contains -Path $header -Pattern 'RTOS_MONITOR_TIMER_HZ\s+10000000UL' -Description 'Monitor must expose the actual 10 MHz timer rate'
Assert-Contains -Path $header -Pattern 'runtime_percent_x100' -Description 'Task snapshot must expose runtime percentage'
Assert-Contains -Path $header -Pattern 'stack_high_water_words' -Description 'Task snapshot must expose stack high water mark'
Assert-Contains -Path $source -Pattern 'rtos_monitor_counter_delta' -Description 'Monitor must implement counter wrap-safe delta'
Assert-Contains -Path $source -Pattern 'uxTaskGetSystemState' -Description 'Monitor must use FreeRTOS task state statistics'
Assert-Contains -Path $source -Pattern 'xTaskGetIdleTaskHandle' -Description 'Monitor must identify the idle task'
Assert-Contains -Path $appConfig -Pattern 'RTOS_MONITOR_ENABLE\s+0U' -Description 'Application configuration must own the monitor default'
Assert-Contains -Path $main -Pattern '#include "app_config\.h"' -Description 'Main must use the shared application configuration'
Assert-Contains -Path $config -Pattern '#include "app_config\.h"' -Description 'FreeRTOS must use the shared application configuration'
Assert-NotContains -Path $main -Pattern '#define RTOS_MONITOR_ENABLE' -Description 'Main must not duplicate the monitor default'
Assert-NotContains -Path $config -Pattern '#define RTOS_MONITOR_ENABLE' -Description 'FreeRTOSConfig must not duplicate the monitor default'
Assert-Contains -Path $config -Pattern 'configUSE_TRACE_FACILITY\s+1' -Description 'FreeRTOS trace facility must be enabled for monitoring'
Assert-Contains -Path $config -Pattern 'configGENERATE_RUN_TIME_STATS\s+1' -Description 'FreeRTOS runtime statistics must be enabled'
Assert-Contains -Path $config -Pattern 'INCLUDE_uxTaskGetStackHighWaterMark\s+1' -Description 'FreeRTOS stack watermark API must be enabled'
Assert-Contains -Path $config -Pattern 'portGET_RUN_TIME_COUNTER_VALUE' -Description 'FreeRTOS must read the hardware runtime counter'
Assert-Contains -Path $main -Pattern 'xTaskCreateStatic\(rtos_monitor_task' -Description 'RTOS monitor task must use static allocation'
Assert-Contains -Path $syscfg -Pattern 'TIMG12' -Description 'SysConfig must reserve TIMG12 for runtime statistics'
Assert-Contains -Path $syscfg -Pattern 'RTOS_MONITOR_TIMER' -Description 'SysConfig timer must have a stable monitor name'
Assert-Contains -Path $syscfg -Pattern 'timerClkDiv\s*=\s*8' -Description 'TIMG12 must use BUSCLK divided by 8'
Assert-Contains -Path $syscfg -Pattern 'timerStartTimer\s*=\s*false' -Description 'TIMG12 must remain stopped when monitoring is disabled'
Assert-Contains -Path $ccsBuild -Pattern 'RtosMonitorEnable' -Description 'CCS build must expose the monitor switch'
Assert-Contains -Path $ccsBuild -Pattern 'rtos_monitor\.c' -Description 'CCS build must compile the monitor source'
Assert-Contains -Path $keilBuild -Pattern 'RtosMonitorEnable' -Description 'Keil build must expose the monitor switch'
Assert-Contains -Path $keilProject -Pattern 'rtos_monitor\.c' -Description 'Keil project must compile the shared monitor source'
Assert-NotContains -Path $main -Pattern 'board_uart_write.*rtos_monitor' -Description 'RTOS monitor must not use UART output'

Write-Output 'RTOS monitor static integration checks passed.'
