[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$config = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h')
$debug = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_debug.h')
$service = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_chassis_service.c')
$lineFollow = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_line_follow.c')
$telemetry = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_telemetry.c')
$runtime = $service + "`n" + $lineFollow

foreach ($pattern in @(
    '#define\s+APP_H723_LINE_FOLLOW_PID_VOFA_TELEMETRY_ENABLE\s+0U',
    '#define\s+APP_H723_LINE_FOLLOW_PID_VOFA_TELEMETRY_INTERVAL_MS\s+10U',
    'APP_H723_LINE_FOLLOW_PID_VOFA_TELEMETRY_ENABLE',
    'H723_VOFA_LINE_FOLLOW_PID_CHANNEL_COUNT\s+13U'
)) {
    if ($config -notmatch $pattern -and $telemetry -notmatch $pattern) {
        throw "Missing line-follow PID VOFA configuration: $pattern"
    }
}

foreach ($field in @(
    'pid_kp', 'pid_ki', 'pid_kd', 'pid_output_limit_mm_s', 'pid_deadband',
    'reset_pid_request', 'params_valid', 'params_rejected_count', 'pid_update_count',
    'pid_dt_s', 'target_position', 'line_position', 'error', 'integral', 'p_out',
    'i_out', 'd_out', 'raw_output', 'pid_output', 'turn_correction_mm_s',
    'base_speed_mm_s', 'left_target_speed_mm_s', 'right_target_speed_mm_s'
)) {
    if ($debug -notmatch [regex]::Escape($field)) {
        throw "Missing line-follow PID debug field: $field"
    }
}

foreach ($pattern in @(
    'PID_Position_Calc',
    's_line_follow\.pid\.params',
    'reset_pid_request\s*=\s*0U',
    'params_rejected_count',
    'debug->line_position',
    'debug->p_out',
    'debug->i_out',
    'debug->d_out',
    'debug->raw_output',
    'debug->turn_correction_mm_s',
    'debug->sequence'
)) {
    if ($runtime -notmatch $pattern) {
        throw "Missing line-follow PID runtime behavior: $pattern"
    }
}

foreach ($pattern in @(
    'HAL_UART_Transmit_DMA\(&huart8',
    'H723_VOFA_LINE_FOLLOW_PID_CHANNEL_COUNT',
    'line_position',
    'p_out',
    'i_out',
    'd_out',
    'raw_output',
    'pid_output',
    'turn_correction_mm_s',
    'base_speed_mm_s',
    'left_target_speed_mm_s',
    'right_target_speed_mm_s',
    'line_strength',
    'sequence'
)) {
    if ($telemetry -notmatch $pattern) {
        throw "Missing line-follow PID VOFA channel behavior: $pattern"
    }
}

Write-Output 'STM32H723 line-follow PID Watch and VOFA static test passed.'
