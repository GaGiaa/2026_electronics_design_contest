[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$headerPath = Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_debug.h'
$header = Get-Content -Raw -LiteralPath $headerPath
foreach ($typeName in @(
    'h723_debug_system_t', 'h723_debug_uart8_t', 'h723_debug_crsf_t',
    'h723_debug_chassis_t', 'h723_debug_fdcan_t', 'h723_debug_jy901s_t',
    'h723_debug_grayscale_t', 'h723_debug_balance_t',
    'h723_debug_bno055_t'
)) {
    if ($header -notmatch "typedef struct \{[\s\S]*?\} $typeName;") {
        throw "Missing debug subgroup type: $typeName"
    }
}

foreach ($member in @(
    'h723_debug_system_t system;', 'h723_debug_uart8_t uart8;',
    'h723_debug_crsf_t crsf;', 'h723_debug_chassis_t chassis;',
    'h723_debug_fdcan_t fdcan;', 'h723_m2006_debug_t m2006[3];',
    'h723_debug_jy901s_t jy901s;', 'h723_debug_grayscale_t grayscale;',
    'h723_debug_single_motor_t single_motor;', 'h723_debug_balance_t balance;',
    'h723_debug_bno055_t bno055;'
)) {
    if ($header -notmatch [regex]::Escape($member)) { throw "Missing grouped debug member: $member" }
}

$sourceFiles = Get-ChildItem (Join-Path $ProjectRoot 'stm32h723_app\App\Src') -Filter '*.c'
$source = ($sourceFiles | ForEach-Object { Get-Content -Raw -LiteralPath $_.FullName }) -join "`n"
foreach ($path in @(
    'g_h723_debug.system.uptime_ms', 'g_h723_debug.uart8.tx_start_count',
    'g_h723_debug.crsf.channels_raw', 'g_h723_debug.chassis.left_target_output_speed_rpm',
    'g_h723_debug.chassis.base_speed_mm_s',
    'g_h723_debug.chassis.line_position',
    'g_h723_debug.chassis.line_turn_correction_mm_s',
    'g_h723_debug.chassis.line_valid',
    'g_h723_debug.chassis.left_target_speed_mm_s',
    'g_h723_debug.chassis.right_target_speed_mm_s',
    'g_h723_debug.fdcan.rx_count', 'g_h723_debug.jy901s.angle_deg',
    'g_h723_debug.jy901s.vehicle_acceleration_g',
    'g_h723_debug.jy901s.vehicle_angular_rate_dps',
    'g_h723_debug.jy901s.vehicle_angle_deg',
    'g_h723_debug.jy901s.gyro_bias_dps',
    'g_h723_debug.jy901s.calibration_status',
    'g_h723_debug.jy901s.calibration_reason',
    'g_h723_debug.grayscale.raw', 'g_h723_debug.grayscale.line_error',
    'g_h723_debug.single_motor.target_output_speed_rpm',
    'g_h723_debug.single_motor.max_target_output_speed_rpm',
    'g_h723_debug.single_motor.control_mode',
    'g_h723_debug.single_motor.target_position_deg',
    'g_h723_debug.single_motor.feedback_position_deg',
    'g_h723_debug.single_motor.position_target_output_speed_rpm',
    'g_h723_debug.single_motor.position_p_out_rpm',
    'g_h723_debug.balance.speed_pid_kp',
    'g_h723_debug.balance.speed_pid_ki',
    'g_h723_debug.balance.speed_pid_dt_s',
    'g_h723_debug.balance.speed_pid_error_rpm',
    'g_h723_debug.balance.speed_pid_integral_output_a',
    'g_h723_debug.balance.speed_pid_output_a',
    'g_h723_debug.pipe_startup.fault',
    'g_h723_debug.pipe_startup.startup_target_position_deg',
    'g_h723_debug.pipe_startup.id3_position_deg',
    'g_h723_debug.pipe_startup.id3_output_speed_rpm',
    'g_h723_debug.ball_position.breakaway_enable',
    'g_h723_debug.ball_position.breakaway_params_valid',
    'debug->breakaway_active',
    'debug->breakaway_trigger_count',
    'debug->breakaway_stall_elapsed_ms',
    'debug->breakaway_offset_deg',
    'g_h723_debug.bno055.angle_deg'
)) {
    if ($source -notmatch [regex]::Escape($path)) { throw "Missing grouped debug access: $path" }
}

if ($source -match 'g_h723_debug\.(boot_count|crsf_channels_raw|fdcan_rx_count|jy901s_angle_deg)\b') {
    throw 'Legacy flat debug member access remains in application source.'
}

Write-Output 'STM32H723 grouped debug layout test passed.'
