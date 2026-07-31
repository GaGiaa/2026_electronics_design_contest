[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

$config = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h')
$debug = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_debug.h')
$service = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_chassis_service.c')

foreach ($pattern in @(
    'APP_H723_TASK2_LINE_FOLLOW_PID_KP',
    'APP_H723_TASK2_LINE_FOLLOW_PID_KI',
    'APP_H723_TASK2_LINE_FOLLOW_PID_KD',
    'APP_H723_TASK2_LINE_FOLLOW_PID_OUTPUT_LIMIT_MM_S',
    'APP_H723_TASK2_LINE_FOLLOW_PID_DEADBAND',
    'APP_H723_TASK456_LINE_FOLLOW_PID_KP',
    'APP_H723_TASK456_LINE_FOLLOW_PID_KI',
    'APP_H723_TASK456_LINE_FOLLOW_PID_KD',
    'APP_H723_TASK456_LINE_FOLLOW_PID_OUTPUT_LIMIT_MM_S',
    'APP_H723_TASK456_LINE_FOLLOW_PID_DEADBAND',
    'APP_H723_LINE_FOLLOW_PID_KP'
)) {
    if ($config -notmatch $pattern) {
        throw "Missing line-follow PID group configuration: $pattern"
    }
}

foreach ($pattern in @(
    'h723_debug_line_follow_params_t',
    'task2_line_follow',
    'task456_line_follow',
    'active_group'
)) {
    if ($debug -notmatch $pattern) {
        throw "Missing line-follow PID group debug field: $pattern"
    }
}

foreach ($pattern in @(
    'APP_H723_LINE_FOLLOW_GROUP_TASK2',
    'APP_H723_LINE_FOLLOW_GROUP_TASK456',
    'task2_line_follow',
    'task456_line_follow',
    'requested_task\s*==\s*2U',
    'requested_task\s*==\s*4U',
    'requested_task\s*==\s*5U',
    'requested_task\s*==\s*6U',
    'app_line_follow_reset'
)) {
    if ($service -notmatch $pattern) {
        throw "Missing line-follow PID group selection: $pattern"
    }
}

Write-Output 'STM32H723 line-follow PID group static test passed.'
