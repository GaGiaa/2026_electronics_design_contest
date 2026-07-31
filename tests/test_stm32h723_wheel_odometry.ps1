[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$appDir = Join-Path $ProjectRoot 'stm32h723_app'
$output = Join-Path $env:TEMP 'stm32h723_wheel_odometry_test.exe'
try {
    & $gcc '-std=c99' '-Wall' '-Wextra' '-Werror' `
        "-I$appDir\App\Inc" `
        "$appDir\App\Src\app_wheel_odometry.c" `
        "$PSScriptRoot\test_stm32h723_wheel_odometry.c" '-lm' '-o' $output
    if ($LASTEXITCODE -ne 0) {
        throw 'STM32H723 wheel odometry test build failed.'
    }
    & $output
    if ($LASTEXITCODE -ne 0) {
        throw 'STM32H723 wheel odometry test execution failed.'
    }
    Write-Output 'STM32H723 wheel odometry algorithm test passed.'
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}
