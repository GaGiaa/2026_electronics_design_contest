[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$appDir = Join-Path $ProjectRoot 'stm32h723_app'
$output = Join-Path $env:TEMP 'stm32h723_telemetry_tx_guard_test.exe'

try {
    & $gcc '-std=c99' '-Wall' '-Wextra' '-Werror' "-I$($appDir)\App\Inc" `
        (Join-Path $appDir 'App\Src\app_telemetry_tx_guard.c') `
        (Join-Path $PSScriptRoot 'test_stm32h723_telemetry_tx_guard.c') '-o' $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 telemetry TX guard test build failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 telemetry TX guard test execution failed.' }
    Write-Output 'STM32H723 telemetry TX guard unit tests passed.'
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}
