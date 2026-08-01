[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

function Require-Text([string] $Content, [string] $Needle, [string] $Message) {
    if ($Content -notmatch [regex]::Escape($Needle)) {
        throw $Message
    }
}

$appRoot = Join-Path $ProjectRoot 'stm32h723_app'
$config = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Inc\app_config.h')
$chassisHeader = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Inc\app_chassis.h')
$chassis = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Src\app_chassis.c')
$service = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Src\app_chassis_service.c')
$debug = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Inc\app_debug.h')
$oled = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Src\app_oled.c')
$project = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'MDK-ARM\stm32h723_app.uvprojx')

foreach ($line in @(
    '#define APP_H723_REMOTE_LINE_FOLLOW_SPEED_PROFILE_MAX_SPEED_MM_S 350.0f',
    '#define APP_H723_REMOTE_LINE_FOLLOW_SPEED_PROFILE_MAX_ACCEL_MM_S2 300.0f',
    '#define APP_H723_REMOTE_LINE_FOLLOW_SPEED_PROFILE_MAX_JERK_MM_S3 1500.0f'
)) {
    Require-Text $config $line "Missing SC high speed-profile configuration: $line"
}

foreach ($line in @(
    'APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW_S_CURVE',
    'APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW'
)) {
    Require-Text $chassisHeader $line "Missing SC high chassis interface: $line"
}

foreach ($line in @(
    'sc_state == 2U',
    'APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW_S_CURVE',
    'APP_H723_REMOTE_LINE_FOLLOW_SPEED_PROFILE_MAX_SPEED_MM_S'
)) {
    Require-Text $chassis $line "Missing SC high mode mapping: $line"
}

foreach ($line in @(
    '#include "app_speed_profile.h"',
    'app_speed_profile_init',
    'h723_remote_line_follow_speed_profile_step',
    'app_speed_profile_step',
    's_remote_line_follow_speed_profile_output.valid',
    '0.0f',
    'APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW_S_CURVE',
    'app_line_follow_step',
    'params_rejected_count',
    'reset_request'
)) {
    Require-Text $service $line "Missing SC high speed-profile service integration: $line"
}

foreach ($line in @(
    'h723_debug_speed_profile_t',
    'speed_profile',
    'planned_speed_mm_s',
    'planned_accel_mm_s2'
)) {
    Require-Text $debug $line "Missing speed-profile debug field: $line"
}

Require-Text $oled 'APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW_S_CURVE' `
    'OLED mode name is missing for SC high S-curve line follow.'
Require-Text $project '<FilePath>../App/Src/app_speed_profile.c</FilePath>' `
    'Speed-profile source absent from Keil project.'

Write-Output 'STM32H723 SC high S-curve line-follow integration check passed.'
