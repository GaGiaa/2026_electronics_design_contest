[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$config = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h')
$telemetry = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_telemetry.c')

if ($config -notmatch '#define\s+APP_H723_TILT_CONTROL_VOFA_TELEMETRY_ENABLE\s+[01]U') {
    throw 'Tilt-control VOFA telemetry enable must be 0U or 1U.'
}
if ($config -notmatch '#define\s+APP_H723_TILT_CONTROL_VOFA_TELEMETRY_INTERVAL_MS\s+[1-9][0-9]*U') {
    throw 'Tilt-control VOFA telemetry interval must be nonzero.'
}
if ($config -notmatch 'APP_H723_TILT_CONTROL_VOFA_TELEMETRY_ENABLE') {
    throw 'Tilt-control VOFA telemetry must participate in the UART8 mutual-exclusion check.'
}
foreach ($pattern in @(
    'H723_VOFA_TILT_CONTROL_CHANNEL_COUNT\s+13U',
    'APP_H723_TILT_CONTROL_VOFA_TELEMETRY_ENABLE',
    'APP_H723_TILT_CONTROL_VOFA_TELEMETRY_INTERVAL_MS',
    'g_h723_debug\.tilt\.target_tilt_deg',
    'g_h723_debug\.tilt\.tilt_deg',
    'g_h723_debug\.tilt\.error_deg',
    'g_h723_debug\.tilt\.pid_p_out_deg_s',
    'g_h723_debug\.tilt\.pid_i_out_deg_s',
    'g_h723_debug\.tilt\.pid_d_out_deg_s',
    'g_h723_debug\.tilt\.pid_rate_deg_s',
    'g_h723_debug\.tilt\.motor_target_position_deg',
    'g_h723_debug\.balance\.feedback_position_deg',
    'g_h723_debug\.balance\.active_target_position_deg',
    'g_h723_debug\.balance\.target_output_speed_rpm',
    'g_h723_debug\.balance\.commanded_current_a',
    '\(float\)g_h723_debug\.tilt\.imu_sample_age_ms'
)) {
    if ($telemetry -notmatch $pattern) {
        throw "Missing tilt-control VOFA telemetry behavior: $pattern"
    }
}

Write-Output 'STM32H723 tilt-control VOFA telemetry static test passed.'
