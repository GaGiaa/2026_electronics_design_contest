[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$config = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h')
$telemetry = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_telemetry.c')

if ($config -notmatch '#define\s+APP_H723_CHASSIS_VOFA_TELEMETRY_ENABLE\s+0U') {
    throw 'Chassis VOFA telemetry must default to disabled.'
}
if ($config -notmatch '#define\s+APP_H723_CHASSIS_VOFA_TELEMETRY_INTERVAL_MS\s+20U') {
    throw 'Chassis VOFA telemetry interval must default to 20 ms.'
}
if ($config -notmatch 'APP_H723_CHASSIS_VOFA_TELEMETRY_ENABLE') {
    throw 'Chassis VOFA telemetry must participate in the UART8 mutual-exclusion check.'
}
foreach ($pattern in @(
    'H723_VOFA_CHASSIS_CHANNEL_COUNT\s+5U',
    'APP_H723_CHASSIS_VOFA_TELEMETRY_ENABLE',
    'left_target_speed_mm_s',
    'right_target_speed_mm_s',
    'feedback_output_speed_rpm\s*\*\s*APP_H723_OUTPUT_RPM_TO_MM_S',
    'g_h723_debug\.grayscale\.black_count'
)) {
    if ($telemetry -notmatch $pattern) {
        throw "Missing chassis telemetry behavior: $pattern"
    }
}

Write-Output 'STM32H723 chassis telemetry static test passed.'
