[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$appConfig = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h')
$service = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_chassis_service.c')
$project = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\MDK-ARM\stm32h723_app.uvprojx')
$expectedDefaults = @{
    'APP_H723_BALANCE_ENABLE' = '1U'
    'APP_H723_BALANCE_MOTOR_ID' = '3U'
    'APP_H723_BALANCE_HOME_SEARCH_OUTPUT_SPEED_RPM' = '-8\.0f'
    'APP_H723_BALANCE_HOME_CURRENT_LIMIT_A' = '1\.0f'
    'APP_H723_BALANCE_HOME_STALL_SPEED_RPM' = '1\.0f'
    'APP_H723_BALANCE_HOME_STALL_CURRENT_RATIO' = '0\.80f'
    'APP_H723_BALANCE_HOME_CONFIRM_MS' = '300U'
    'APP_H723_BALANCE_HOME_TIMEOUT_MS' = '12000U'
    'APP_H723_BALANCE_POSITION_MIN_DEG' = '0\.0f'
    'APP_H723_BALANCE_POSITION_ACTIVE_MIN_DEG' = '5\.0f'
    'APP_H723_BALANCE_POSITION_MAX_DEG' = '275\.0f'
    'APP_H723_BALANCE_POSITION_MAX_OUTPUT_SPEED_RPM' = '550\.0f'
    'APP_H723_BALANCE_POSITION_CURRENT_LIMIT_A' = '10\.0f'
}
foreach ($name in $expectedDefaults.Keys) {
    if ($appConfig -notmatch "#define\s+$name\s+$($expectedDefaults[$name])") {
        throw "Balance default $name is missing or changed."
    }
}
if ($service -notmatch 'h723_balance_service_step\(now_ms, output_current_A\)') {
    throw 'Balance controller is not connected to the chassis service current slots.'
}
if ($project -notmatch '<FilePath>\.\./App/Src/app_balance\.c</FilePath>') {
    throw 'Keil project does not compile app_balance.c.'
}

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$output = Join-Path $env:TEMP 'stm32h723_balance_test.exe'
$appDir = Join-Path $ProjectRoot 'stm32h723_app'
try {
    & $gcc '-std=c99' '-Wall' '-Wextra' '-Werror' "-I$appDir\App\Inc" "-I$ProjectRoot\shared\pid" `
        "$ProjectRoot\shared\pid\pid.c" "$appDir\App\Src\app_balance.c" `
        "$PSScriptRoot\test_stm32h723_balance.c" '-lm' '-o' $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 balance test build failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 balance test execution failed.' }
    Write-Output 'STM32H723 balance unit tests passed.'
}
finally { Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue }
