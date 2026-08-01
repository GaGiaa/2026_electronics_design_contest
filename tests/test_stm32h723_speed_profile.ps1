[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$output = Join-Path $env:TEMP 'stm32h723_speed_profile_test.exe'
try {
    & $gcc '-std=c99' '-Wall' '-Wextra' '-Werror' `
        "-I$ProjectRoot\stm32h723_app\App\Inc" `
        "$ProjectRoot\stm32h723_app\App\Src\app_speed_profile.c" `
        "$PSScriptRoot\test_stm32h723_speed_profile.c" '-lm' '-o' $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 speed-profile test build failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 speed-profile test execution failed.' }
    Write-Output 'STM32H723 speed-profile unit tests passed.'
}
finally { Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue }
