[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

$service = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_chassis_service.c')
$task = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_task56.c')
$header = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_task56.h')
$config = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h')
$debug = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_debug.h')
$project = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\MDK-ARM\stm32h723_app.uvprojx')

foreach ($pattern in @(
    '#include\s+"app_task56\.h"',
    'requested_task\s*==\s*5U',
    'requested_task\s*==\s*6U',
    'app_task56_start',
    'app_task56_abort',
    'app_task56_step',
    'app_task_menu_finish_execution',
    'APP_CHASSIS_MODE_LINE_FOLLOW'
)) {
    if ($service -notmatch $pattern) {
        throw "Missing task 5/6 service integration: $pattern"
    }
}

if ($task -match 'black_count') {
    throw 'Task 5/6 state machine must not use grayscale black count.'
}

foreach ($pattern in @(
    'APP_H723_TASK56_CRUISE_SPEED_MM_S',
    'APP_H723_TASK56_ACCEL_MM_S2',
    'APP_H723_TASK56_DECEL_MM_S2',
    'APP_H723_TASK56_RUN_TIMEOUT_MS',
    'APP_TASK56_PHASE_STOPPED'
)) {
    if ($task -notmatch $pattern) {
        throw "Missing task 5/6 behavior: $pattern"
    }
}

if ($config -notmatch '#define\s+APP_H723_TASK56_RUN_TIMEOUT_MS\s+30000U') {
    throw 'Task 5/6 timeout must default to 30000 ms.'
}
if ($header -notmatch 'void\s+app_task56_start') {
    throw 'Missing task 5/6 start interface.'
}
if ($debug -notmatch 'h723_debug_task56_t') {
    throw 'Missing task 5/6 debug snapshot.'
}
if ($project -notmatch '<FilePath>\.\./App/Src/app_task56\.c</FilePath>') {
    throw 'Task 5/6 source is missing from the Keil project.'
}

Write-Output 'STM32H723 task 5/6 integration checks passed.'
