[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$service = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_chassis_service.c')

foreach ($pattern in @(
    '#include\s+"app_line_follow\.h"',
    'static app_line_follow_state_t s_line_follow;',
    'app_line_follow_init\(&s_line_follow',
    'APP_CHASSIS_MODE_LINE_FOLLOW',
    'app_line_follow_step\(&s_line_follow',
    'app_line_follow_reset\(&s_line_follow',
    'g_h723_debug\.grayscale\.line_position'
)) {
    if ($service -notmatch $pattern) {
        throw "Missing line-follow service integration: $pattern"
    }
}

if ($service -match '\.black_count\s*=') {
    throw 'Line-follow service must not pass black_count as a stop gate.'
}

foreach ($pattern in @(
    'app_m2006_position_tracker_update',
    's_position_tracker',
    'APP_H723_SINGLE_MOTOR_POSITION_PID_PERIOD_MS'
)) {
    if ($service -notmatch $pattern) {
        throw "Current single-motor position-loop integration was removed: $pattern"
    }
}

Write-Output 'STM32H723 line-follow service static test passed.'
