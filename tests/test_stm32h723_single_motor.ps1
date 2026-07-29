[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$output = Join-Path $env:TEMP 'stm32h723_single_motor_test.exe'
$appDir = Join-Path $ProjectRoot 'stm32h723_app'
try {
    & $gcc '-std=c99' '-Wall' '-Wextra' '-Werror' "-I$appDir\App\Inc" "-I$ProjectRoot\shared\pid" `
        "$ProjectRoot\shared\pid\pid.c" "$appDir\App\Src\app_single_motor.c" `
        "$PSScriptRoot\test_stm32h723_single_motor.c" '-lm' '-o' $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 single motor test build failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 single motor test execution failed.' }
    Write-Output 'STM32H723 single motor unit tests passed.'
}
finally { Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue }
