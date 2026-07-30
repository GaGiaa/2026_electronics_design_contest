[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$appConfig = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h')
$service = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_chassis_service.c')
if ($appConfig -notmatch '#define\s+APP_H723_CHASSIS_ACTUATION_ENABLE\s+(?:0U|1U)') {
    throw 'Chassis actuation must be configured as 0U or 1U.'
}
if ($service -notmatch '(?s)if\s*\(APP_H723_CHASSIS_ACTUATION_ENABLE\s*==\s*0U\)\s*\{\s*output_current_A\[index\]\s*=\s*0\.0f;') {
    throw 'Disabled chassis actuation must force zero motor current.'
}

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$output = Join-Path $env:TEMP 'stm32h723_chassis_test.exe'
try {
    & $gcc '-std=c99' '-Wall' '-Wextra' '-Werror' "-I$ProjectRoot\stm32h723_app\App\Inc" "-I$ProjectRoot\shared\pid" `
        "$ProjectRoot\shared\pid\pid.c" "$ProjectRoot\stm32h723_app\App\Src\app_crsf.c" `
        "$ProjectRoot\stm32h723_app\App\Src\app_chassis.c" "$ProjectRoot\stm32h723_app\App\Src\app_m2006.c" `
        "$PSScriptRoot\test_stm32h723_chassis.c" '-lm' '-o' $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 chassis test build failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 chassis test execution failed.' }
    Write-Output 'STM32H723 chassis unit tests passed.'
}
finally { Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue }
