[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$config = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h')
$telemetry = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_telemetry.c')

foreach ($pattern in @(
    '#define\s+APP_H723_BALL_POSITION_VOFA_TELEMETRY_ENABLE\s+0U',
    '#define\s+APP_H723_BALL_POSITION_VOFA_TELEMETRY_INTERVAL_MS\s+25U',
    'APP_H723_BALL_POSITION_VOFA_TELEMETRY_ENABLE'
)) {
    if ($config -notmatch $pattern) {
        throw "Missing ball-position VOFA configuration: $pattern"
    }
}

foreach ($pattern in @(
    'H723_VOFA_BALL_POSITION_CHANNEL_COUNT\s+13U',
    'APP_H723_BALL_POSITION_VOFA_TELEMETRY_ENABLE',
    'APP_H723_BALL_POSITION_VOFA_TELEMETRY_INTERVAL_MS',
    'g_h723_debug\.ball_position\.target_mm',
    'g_h723_debug\.ball_position\.measured_mm',
    'g_h723_debug\.ball_position\.error_mm',
    'g_h723_debug\.ball_position\.pid_p_out_deg',
    'g_h723_debug\.ball_position\.pid_i_out_deg',
    'g_h723_debug\.ball_position\.pid_d_out_deg',
    'g_h723_debug\.ball_position\.pid_output_deg',
    'g_h723_debug\.ball_position\.target_tilt_deg',
    '\(float\)g_h723_debug\.ball_position\.vision_age_ms',
    '\(float\)g_h723_debug\.ball_position\.vision_valid',
    '\(float\)g_h723_debug\.ball_position\.state',
    '\(float\)g_h723_debug\.ball_position\.fault',
    '\(float\)g_h723_debug\.pipe_startup\.calibration_valid'
)) {
    if ($telemetry -notmatch $pattern) {
        throw "Missing ball-position VOFA telemetry behavior: $pattern"
    }
}

Write-Output 'STM32H723 ball-position VOFA telemetry static test passed.'
