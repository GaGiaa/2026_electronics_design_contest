[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$output = Join-Path $env:TEMP 'stm32h723_position_service_test.exe'
$support = Join-Path $ProjectRoot 'tests\test_support\stm32h723_service'
$appDir = Join-Path $ProjectRoot 'stm32h723_app'
try {
    & $gcc '-std=c99' '-Wall' '-Wextra' '-Werror' '-Wno-unused-function' '-DAPP_H723_SINGLE_MOTOR_PID_DEBUG_ENABLE=1U' "-I$support" "-I$appDir\App\Inc" "-I$ProjectRoot\shared\pid" `
        "-I$PSScriptRoot\stubs" "$ProjectRoot\shared\pid\pid.c" "$appDir\App\Src\app_crsf.c" `
        "$appDir\App\Src\app_chassis.c" "$appDir\App\Src\app_task_menu.c" `
        "$appDir\App\Src\app_task2.c" "$appDir\App\Src\app_task4.c" `
        "$appDir\App\Src\app_task56.c" `
        "$appDir\App\Src\app_buttons.c" "$PSScriptRoot\stubs\gpio_state.c" `
        "$appDir\App\Src\app_m2006.c" `
        "$appDir\App\Src\app_line_follow.c" "$appDir\App\Src\app_balance.c" `
        "$appDir\App\Src\app_tilt_control.c" `
        "$appDir\App\Src\app_single_motor.c" "$appDir\App\Src\app_chassis_service.c" `
        "$PSScriptRoot\test_stm32h723_position_service.c" '-lm' '-o' $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 position service test build failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 position service test execution failed.' }
    Write-Output 'STM32H723 position service integration test passed.'
}
finally { Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue }
