[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

$service = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_chassis_service.c')
$oled = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_oled.c')
$task3 = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_task3.c')
$debug = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_debug.h')

foreach ($pattern in @(
    'requested_task\s*==\s*3U',
    'app_task3_start\(',
    'app_task3_step\(',
    'APP_TASK_MENU_BALANCE_SETUP_ID',
    'h723_ball_position_capture_target',
    's_control_output\.remote_takeover',
    's_control_output\.sb_state\s*==\s*1U'
)) {
    if ($service -notmatch $pattern) {
        throw "Missing task 3 integration behavior: $pattern"
    }
}

if ($task3 -notmatch 'APP_TASK3_PHASE_WAIT_FINISH_KEY' -or
    $task3 -notmatch 'measured_mm\s*>=\s*task->config.switch_threshold_mm') {
    throw 'Task 3 does not implement the 175 mm threshold transition.'
}
if ($oled -match 'TASK 3 N/A|NOT IMPLEMENTED' -or
    $oled -notmatch 'TASK 3' -or
    $oled -notmatch 'BALL SET' -or
    $oled -notmatch 'g_h723_debug\.task3') {
    throw 'OLED does not render task 3 and BALL SET runtime pages.'
}
if ($debug -notmatch 'h723_debug_task3_t' -or
    $debug -notmatch 'capture_status') {
    throw 'Debug state does not expose task 3 and capture status.'
}

Write-Output 'STM32H723 task 3 integration checks passed.'
