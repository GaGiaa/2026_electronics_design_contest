param([string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$service = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_chassis_service.c')
$debug = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_debug.h')
$oled = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_oled.c')
$task2 = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_task2.c')
foreach ($pattern in @(
    '#include\s+"app_task2\.h"',
    'app_task_menu_take_execution_request',
    'app_task_menu_finish_execution',
    'app_task2_start',
    'app_task2_abort',
    'app_task2_step'
)) { if ($service -notmatch $pattern) { throw "Missing task 2 service integration: $pattern" } }
if ($service -notmatch 'task2_was_running' -or
    $service -notmatch 's_task2\.phase\s*==\s*APP_TASK2_PHASE_STOPPED') {
    throw 'Task 2 service does not detect the running-to-stopped completion transition.'
}
if ($task2 -notmatch 'APP_H723_TASK2_SPEED_MM_S') { throw 'Task 2 does not use configured fixed speed.' }
if ($task2 -notmatch 'APP_H723_TASK2_STOP_BLACK_COUNT') { throw 'Task 2 does not use black-count stop threshold.' }
foreach ($pattern in @(
    'APP_H723_TASK2_CRUISE_SPEED_MM_S',
    'APP_H723_TASK2_APPROACH_SPEED_MM_S',
    'APP_H723_TASK2_REARM_DISTANCE_MM',
    'APP_H723_TASK2_RUN_TIMEOUT_MS',
    'task2_restart_requested'
)) { if ($service -match $pattern -or $task2 -match $pattern) { throw "Obsolete task 2 logic remains: $pattern" } }
if ($debug -notmatch 'h723_debug_task2_t') { throw 'Missing task 2 debug snapshot.' }
if ($oled -notmatch 'g_h723_debug\.task2') { throw 'OLED does not render task 2 status.' }
Write-Output 'STM32H723 task 2 integration checks passed.'
