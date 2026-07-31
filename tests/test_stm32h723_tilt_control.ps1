[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$output = Join-Path $env:TEMP 'stm32h723_tilt_control_test.exe'
try {
    & $gcc '-std=c99' '-Wall' '-Wextra' '-Werror' `
        "-I$ProjectRoot\stm32h723_app\App\Inc" "-I$ProjectRoot\shared\pid" `
        "$ProjectRoot\shared\pid\pid.c" `
        "$ProjectRoot\stm32h723_app\App\Src\app_tilt_control.c" `
        "$PSScriptRoot\test_stm32h723_tilt_control.c" '-lm' '-o' $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 tilt-control test build failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 tilt-control test execution failed.' }
    Write-Output 'STM32H723 tilt-control unit tests passed.'
}
finally { Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue }
