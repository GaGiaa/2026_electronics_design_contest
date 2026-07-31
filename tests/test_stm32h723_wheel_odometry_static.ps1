[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

function Assert-Contains([string] $Text, [string] $Pattern, [string] $Description) {
    if ($Text -notmatch $Pattern) {
        throw "Missing ${Description}: $Pattern"
    }
}

$config = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h')
$debug = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_debug.h')
$service = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_chassis_service.c')
$telemetry = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_telemetry.c')
$project = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\MDK-ARM\stm32h723_app.uvprojx')

foreach ($pattern in @(
    '#define\s+APP_H723_WHEEL_ODOMETRY_VOFA_TELEMETRY_ENABLE\s+0U',
    '#define\s+APP_H723_WHEEL_ODOMETRY_VOFA_TELEMETRY_INTERVAL_MS\s+10U',
    '#define\s+APP_H723_WHEEL_ODOMETRY_TRACK_WIDTH_MM\s+205\.0f',
    'APP_H723_WHEEL_ODOMETRY_VOFA_TELEMETRY_ENABLE',
    'APP_H723_WHEEL_ODOMETRY_VOFA_TELEMETRY_INTERVAL_MS'
)) {
    Assert-Contains $config $pattern 'wheel odometry configuration'
}

foreach ($field in @(
    'wheel_diameter_mm', 'track_width_mm', 'left_encoder_sign', 'right_encoder_sign',
    'reset_request', 'params_valid', 'params_rejected_count', 'initialized', 'valid',
    'reset_count', 'rebaseline_count', 'sample_count', 'left_encoder', 'right_encoder',
    'left_motor_counts', 'right_motor_counts', 'left_wheel_distance_mm',
    'right_wheel_distance_mm', 'delta_left_mm', 'delta_right_mm', 'delta_distance_mm',
    'delta_yaw_deg', 'x_mm', 'y_mm', 'yaw_deg', 'yaw_deg_continuous', 'linear_speed_mm_s',
    'angular_speed_deg_s', 'left_feedback_age_ms', 'right_feedback_age_ms'
)) {
    Assert-Contains $debug ([regex]::Escape($field)) "Watch field $field"
}

foreach ($pattern in @(
    'app_wheel_odometry\.h',
    'app_wheel_odometry_set_config',
    'app_wheel_odometry_step',
    'APP_H723_WHEEL_ODOMETRY_WHEEL_DIAMETER_MM',
    'APP_H723_WHEEL_ODOMETRY_TRACK_WIDTH_MM',
    'APP_H723_WHEEL_ODOMETRY_LEFT_ENCODER_SIGN',
    'APP_H723_WHEEL_ODOMETRY_RIGHT_ENCODER_SIGN',
    'reset_request\s*=\s*0U',
    'app_m2006_position_tracker_motor_counts\(&s_position_tracker\[0U\]\)',
    'app_m2006_position_tracker_motor_counts\(&s_position_tracker\[1U\]\)'
)) {
    Assert-Contains $service $pattern 'wheel odometry runtime behavior'
}

Assert-Contains $telemetry 'H723_VOFA_WHEEL_ODOMETRY_CHANNEL_COUNT\s+15U' '15-channel VOFA definition'
Assert-Contains $telemetry 'HAL_UART_Transmit_DMA\(&huart8' 'UART8 DMA transmission'
foreach ($channel in @(
    'left_wheel_distance_mm', 'right_wheel_distance_mm', 'delta_left_mm',
    'delta_right_mm', 'delta_distance_mm', 'delta_yaw_deg', 'x_mm', 'y_mm',
    'yaw_deg', 'linear_speed_mm_s', 'angular_speed_deg_s',
    'left_feedback_age_ms', 'right_feedback_age_ms', 'valid', 'sample_count'
)) {
    Assert-Contains $telemetry ([regex]::Escape($channel)) "VOFA channel $channel"
}

Assert-Contains $project '<FilePath>../App/Src/app_wheel_odometry.c</FilePath>' 'Keil wheel odometry source'
Write-Output 'STM32H723 wheel odometry static test passed.'
