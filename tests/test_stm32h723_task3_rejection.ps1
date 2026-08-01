[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

$service = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_chassis_service.c')
$oled = Get-Content -Raw (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_oled.c')

foreach ($pattern in @(
    'requested_task\s*==\s*3U',
    'app_task3_start\(',
    'app_task3_step\(',
    'APP_TASK3_PHASE_FINISHED_WAIT_KEY',
    'app_task2_abort\(&s_task2\)',
    'app_task4_abort\(&s_task4\)',
    'app_task56_abort\(&s_task56\)'
)) {
    if ($service -notmatch $pattern) {
        throw "Missing task 3 rejection safeguard: $pattern"
    }
}

if ($oled -notmatch 'TASK 3' -or $oled -notmatch 'TIME:%lu\.%03lus') {
    throw 'OLED does not render task 3 and its cumulative timer.'
}

Write-Output 'STM32H723 task 3 rejection checks passed.'
