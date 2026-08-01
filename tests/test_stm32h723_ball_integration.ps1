[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

function Require-Text([string] $Content, [string] $Needle, [string] $Message) {
    if ($Content -notmatch [regex]::Escape($Needle)) { throw $Message }
}

$appRoot = Join-Path $ProjectRoot 'stm32h723_app'
$config = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Inc\app_config.h')
$freertos = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'Core\Src\freertos.c')
$k230Header = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Inc\app_k230_service.h')
$chassis = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Src\app_chassis_service.c')
$ballControl = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Src\app_ball_position_control.c')
$debug = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Inc\app_debug.h')
$oled = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Src\app_oled.c')
$project = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'MDK-ARM\stm32h723_app.uvprojx')

foreach ($line in @(
    '#define APP_H723_K230_UART2_ENABLE 1U',
    '#define APP_H723_K230_UART2_TEST_ENABLE 0U',
    '#define APP_H723_BALL_POSITION_PERIOD_MS 20U',
    '#define APP_H723_BALL_POSITION_SAMPLE_MAX_AGE_MS 100U',
    '#define APP_H723_PIPE_STARTUP_CALIBRATION_POSITION_DEG 134.0f',
    '#define APP_H723_PIPE_STARTUP_CALIBRATION_SETTLE_MS 200U',
    '#define APP_H723_PIPE_STARTUP_CALIBRATION_MOVE_TIMEOUT_MS 10000U'
)) { Require-Text $config $line "Missing ball-control configuration: $line" }

foreach ($line in @(
    'APP_H723_K230_UART2_ENABLE == 1U',
    'h723_k230_service_init',
    'h723_k230_service_step'
)) { Require-Text $freertos $line "K230 receive task must use the production enable switch: $line" }

Require-Text $k230Header 'h723_k230_service_get_snapshot' 'K230 service snapshot API is missing.'

foreach ($line in @(
    '#include "app_ball_position_control.h"',
    '#include "app_pipe_startup.h"',
    'app_pipe_startup_step',
    'APP_PIPE_STARTUP_STATE_MOVE_TO_CALIBRATION_POSITION',
    'APP_H723_PIPE_STARTUP_CALIBRATION_POSITION_DEG',
    'app_ball_position_control_step',
    'h723_k230_service_get_snapshot',
    'output_current_A[index] = 0.0f;'
)) { Require-Text $chassis $line "Missing ball-control chassis integration: $line" }
Require-Text $ballControl 'PID_Position_Calc_DerivativeOnMeasurement' `
    'Ball-position PID must use derivative-on-measurement calculation.'

foreach ($line in @(
    'app_k230_pixel_to_ball_mm',
    'ball_sample.pixel_x',
    'ball_sample.ball_position_mm',
    'input.feedback_position_deg - s_balance.zero_offset_deg',
    '(155.0f - motor_position_deg) * 0.0747f',
    'input.feedback_valid'
)) { Require-Text $chassis $line "Missing vision perspective correction integration: $line" }

foreach ($line in @(
    'hold_position_mm',
    'hold_motor_position_deg',
    'use_hold_position_map',
    'target_motor_position_deg',
    'breakaway_enable',
    'breakaway_pulse_deg',
    'breakaway_stall_time_ms',
    'breakaway_min_motion_mm',
    'breakaway_duration_ms',
    'breakaway_cooldown_ms',
    'breakaway_trigger_count',
    'breakaway_stall_elapsed_ms'
)) { Require-Text ($chassis + "`n" + $debug) $line "Missing ball-control tuning integration: $line" }

Require-Text $chassis 's_ball_position_dynamic_config.use_hold_position_map = false;' `
    'Dynamic ball control must use a fixed base instead of the static hold map.'
Require-Text $chassis 's_ball_position_dynamic_config.pid_params = s_ball_position_config.pid_params;' `
    'Dynamic ball control must inherit the tuned static PID defaults.'
if ($chassis -match 's_ball_position_dynamic_config\s*=\s*s_ball_position_config\s*;') {
    throw 'Dynamic ball control must not copy the static three-point configuration.'
}

foreach ($line in @(
    'g_h723_debug.ball_position.enable = 1U;',
    'g_h723_debug.ball_position.target_mm = 125.0f;'
)) { Require-Text $chassis $line "Missing tuned static ball-position default: $line" }

foreach ($line in @(
    '.kp = 2.0f',
    '.kd = 0.8f',
    '.output_limit = 360.0f',
    '.deadband = 0.5f',
    '.output_limit_deg = 360.0f',
    '.deadband_mm = 0.5f',
    '.engage_error_mm = 0.5f',
    '.release_error_mm = 0.5f',
    '.breakaway_enable = false',
    '.breakaway_pulse_deg = 10.0f'
)) { Require-Text $ballControl $line "Missing tuned ball-position PID default: $line" }

foreach ($line in @(
    'h723_debug_ball_position_t',
    'h723_debug_pipe_startup_t',
    'startup_target_position_deg',
    'id3_output_speed_rpm',
    'h723_debug_ball_position_t ball_position;',
    'h723_debug_pipe_startup_t pipe_startup;'
)) { Require-Text $debug $line "Missing ball-control debug field: $line" }

foreach ($line in @('MOVE PIPE', 'TARGET:%.0f', 'WAIT:SETTLE')) {
    Require-Text $oled $line "Missing pipe startup OLED prompt: $line"
}

foreach ($line in @(
    '<FilePath>../App/Src/app_ball_position_control.c</FilePath>',
    '<FilePath>../App/Src/app_pipe_startup.c</FilePath>'
)) { Require-Text $project $line "Ball-control source absent from Keil project: $line" }

Write-Output 'STM32H723 ball-control integration static test passed.'
