param([string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$service = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_chassis_service.c')
$task4   = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_task4.c')
$header  = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_task4.h')
$config  = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h')
$debug   = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_debug.h')

# Service integration: chassis_service must wire task4
foreach ($pattern in @(
    '#include\s+"app_task4\.h"',
    'app_task_menu_take_execution_request',
    'app_task4_start',
    'app_task4_abort',
    'app_task4_step',
    'app_task_menu_finish_execution',
    'APP_CHASSIS_MODE_LINE_FOLLOW',
    'app_line_follow_step',
    'h723_average_distance_mm',
    '\.distance_mm\s*='
)) {
    if ($service -notmatch $pattern) {
        throw "Missing task 4 service integration: $pattern"
    }
}

# Task 4 implementation: must use distance-based behavior
foreach ($pattern in @(
    'APP_H723_TASK4_CRUISE_SPEED_MM_S',
    'APP_H723_TASK4_ACCEL_DISTANCE_MM',
    'APP_H723_TASK4_BRAKE_DISTANCE_MM',
    'APP_H723_TASK4_RUN_TIMEOUT_MS',
    'APP_H723_TASK4_A_TO_B_DISTANCE_MM',
    'APP_TASK4_PHASE_STOPPED',
    'traveled_distance_mm',
    'start_distance_mm'
)) {
    if ($task4 -notmatch $pattern) {
        throw "Missing task 4 behavior: $pattern"
    }
}

# Header: distance fields must be present
if ($header -notmatch 'float\s+distance_mm') {
    throw 'Missing distance_mm in task4 types.'
}
if ($header -notmatch 'start_distance_mm') {
    throw 'Missing start_distance_mm in task4 state.'
}
if ($header -notmatch 'traveled_distance_mm') {
    throw 'Missing traveled_distance_mm in task4 state.'
}

# Config: timeout changed to 8000ms
if ($config -notmatch '#define\s+APP_H723_TASK4_RUN_TIMEOUT_MS\s+8000U') {
    throw 'Task 4 timeout must default to 8000 ms.'
}
if ($config -notmatch 'APP_H723_TASK4_BRAKE_DISTANCE_MM') {
    throw 'Task 4 brake distance macro is missing.'
}
if ($config -notmatch 'APP_H723_TASK4_ACCEL_DISTANCE_MM') {
    throw 'Task 4 accel distance macro is missing.'
}

# Debug: distance_mm field must exist
if ($debug -notmatch 'h723_debug_task4_t') {
    throw 'Missing task 4 debug snapshot.'
}
if ($debug -notmatch 'distance_mm') {
    throw 'Missing distance_mm in task4 debug struct.'
}

# Guard: no obsolete multi-phase approach/rearm patterns
foreach ($pattern in @(
    'APP_H723_TASK4_APPROACH_SPEED_MM_S',
    'APP_H723_TASK4_REARM_DISTANCE_MM',
    'task4_restart_requested'
)) {
    if (($service -match $pattern) -or ($task4 -match $pattern)) {
        throw "Obsolete task 4 pattern detected: $pattern"
    }
}

Write-Output 'STM32H723 task 4 integration checks passed.'
