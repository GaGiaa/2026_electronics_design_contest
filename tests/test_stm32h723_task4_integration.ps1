param([string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$service = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_chassis_service.c')
$task4 = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_task4.c')
$config = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h')
$debug = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_debug.h')
foreach ($pattern in @(
    '#include\s+"app_task4\.h"',
    'app_task_menu_take_execution_request',
    'app_task4_start',
    'app_task4_abort',
    'app_task4_step',
    'app_task_menu_finish_execution',
    'APP_CHASSIS_MODE_LINE_FOLLOW',
    'app_line_follow_step'
)) { if ($service -notmatch $pattern) { throw "Missing task 4 service integration: $pattern" } }
foreach ($pattern in @(
    'APP_H723_TASK4_CRUISE_SPEED_MM_S',
    'APP_H723_TASK4_ACCEL_MM_S2',
    'APP_H723_TASK4_DECEL_MM_S2',
    'APP_H723_TASK4_RUN_TIMEOUT_MS',
    'APP_TASK4_PHASE_STOPPED'
)) { if ($task4 -notmatch $pattern) { throw "Missing task 4 behavior: $pattern" } }
if ($config -notmatch '#define\s+APP_H723_TASK4_RUN_TIMEOUT_MS\s+10000U') {
    throw 'Task 4 timeout must default to 10000 ms.'
}
if ($debug -notmatch 'h723_debug_task4_t') { throw 'Missing task 4 debug snapshot.' }
Write-Output 'STM32H723 task 4 integration checks passed.'
