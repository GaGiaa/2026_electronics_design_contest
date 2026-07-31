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
    'app_task_menu_finish_execution\(\)',
    'app_task2_abort\(&s_task2\)',
    'app_task4_abort\(&s_task4\)',
    'app_task56_abort\(&s_task56\)'
)) {
    if ($service -notmatch $pattern) {
        throw "Missing task 3 rejection safeguard: $pattern"
    }
}

if ($oled -notmatch 'TASK 3 N/A') {
    throw 'OLED does not identify task 3 as unavailable.'
}

Write-Output 'STM32H723 task 3 rejection checks passed.'
