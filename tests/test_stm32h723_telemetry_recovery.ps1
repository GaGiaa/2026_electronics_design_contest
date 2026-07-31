[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$config = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h')
$debug = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_debug.h')
$telemetry = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_telemetry.c')
$project = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\MDK-ARM\stm32h723_app.uvprojx')

if ($config -notmatch '#define\s+APP_H723_UART8_TX_TIMEOUT_MS\s+20U') {
    throw 'UART8 telemetry recovery timeout must default to 20 ms.'
}
foreach ($pattern in @(
    'tx_started_ms',
    'tx_timeout_count',
    'tx_recovery_count',
    'tx_recovery_failure_count',
    'hal_g_state',
    'hal_error_code'
)) {
    if ($debug -notmatch $pattern) { throw "Missing UART8 recovery diagnostic: $pattern" }
}
foreach ($pattern in @(
    'app_telemetry_tx_guard_reserve',
    'app_telemetry_tx_guard_timed_out',
    'HAL_UART_AbortTransmit\(&huart8\)',
    'app_telemetry_tx_guard_complete'
)) {
    if ($telemetry -notmatch $pattern) { throw "Missing UART8 recovery behavior: $pattern" }
}
$reserveOffset = $telemetry.IndexOf('app_telemetry_tx_guard_reserve')
$dmaOffset = $telemetry.IndexOf('HAL_UART_Transmit_DMA')
if ($reserveOffset -lt 0 -or $dmaOffset -lt 0 -or $reserveOffset -gt $dmaOffset) {
    throw 'UART8 telemetry must reserve the software guard before starting DMA.'
}
if ($project -notmatch '<FilePath>../App/Src/app_telemetry_tx_guard\.c</FilePath>') {
    throw 'Keil project does not compile the telemetry TX guard.'
}

Write-Output 'STM32H723 telemetry recovery static test passed.'
