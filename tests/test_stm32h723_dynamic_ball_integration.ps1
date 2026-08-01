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
$debug = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Inc\app_debug.h')
$service = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Src\app_chassis_service.c')
$project = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'MDK-ARM\stm32h723_app.uvprojx')

foreach ($line in @(
    'APP_H723_REMOTE_BALL_SPEED_PROFILE_MAX_SPEED_MM_S',
    'APP_H723_REMOTE_BALL_SPEED_PROFILE_MAX_ACCEL_MM_S2',
    'APP_H723_REMOTE_BALL_SPEED_PROFILE_MAX_JERK_MM_S3'
)) { Require-Text $config $line "Missing remote ball speed-profile configuration: $line" }

foreach ($line in @(
    'h723_debug_speed_profile_t',
    'h723_debug_ball_position_dynamic_t',
    'ball_position_dynamic',
    'speed_profile',
    'active_profile'
)) { Require-Text $debug $line "Missing dynamic ball debug interface: $line" }

foreach ($line in @(
    '#include "app_speed_profile.h"',
    'app_speed_profile_step',
    'APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW_BALL',
    's_ball_position_dynamic_control',
    'g_h723_debug.ball_position_dynamic'
)) { Require-Text $service $line "Missing dynamic ball chassis integration: $line" }

Require-Text $project '<FilePath>../App/Src/app_speed_profile.c</FilePath>' `
    'Speed-profile source absent from Keil project.'

Write-Output 'STM32H723 dynamic ball integration static test passed.'
