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
$debug = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Inc\app_debug.h')
$oled = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Src\app_oled.c')
$project = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'MDK-ARM\stm32h723_app.uvprojx')

foreach ($line in @(
    '#define APP_H723_K230_UART2_ENABLE 1U',
    '#define APP_H723_K230_UART2_TEST_ENABLE 0U',
    '#define APP_H723_BALL_POSITION_PERIOD_MS 25U',
    '#define APP_H723_BALL_POSITION_SAMPLE_MAX_AGE_MS 100U'
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
    'app_ball_position_control_step',
    'h723_k230_service_get_snapshot',
    'output_current_A[index] = 0.0f;'
)) { Require-Text $chassis $line "Missing ball-control chassis integration: $line" }

foreach ($line in @(
    'ball_input.vision_frame_count = ball_sample.valid_frame_count;',
    'ball_input.vision_sample_ms = ball_sample.last_frame_ms;',
    'hold_position_mm',
    'breakaway_negative_tilt_deg',
    'velocity_gain_deg_per_mm_s',
    'output->velocity_damping_tilt_deg'
)) { Require-Text ($chassis + "`n" + $debug) $line "Missing ball-control tuning integration: $line" }

foreach ($line in @(
    'h723_debug_ball_position_t',
    'h723_debug_pipe_startup_t',
    'h723_debug_ball_position_t ball_position;',
    'h723_debug_pipe_startup_t pipe_startup;'
)) { Require-Text $debug $line "Missing ball-control debug field: $line" }

foreach ($line in @(
    'CALIBRATE PIPE',
    'B1:CAPTURE',
    'B2:SKIP'
)) { Require-Text $oled $line "Missing pipe calibration OLED prompt: $line" }

foreach ($line in @(
    '<FilePath>../App/Src/app_ball_position_control.c</FilePath>',
    '<FilePath>../App/Src/app_pipe_startup.c</FilePath>'
)) { Require-Text $project $line "Ball-control source absent from Keil project: $line" }

Write-Output 'STM32H723 ball-control integration static test passed.'
