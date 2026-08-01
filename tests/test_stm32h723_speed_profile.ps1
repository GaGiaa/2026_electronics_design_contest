param([string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$compiler = Get-Command gcc -ErrorAction Stop
$output = Join-Path $env:TEMP 'test_stm32h723_speed_profile.exe'
try {
    & $compiler.Source -std=c11 -Wall -Wextra -Werror `
        -I (Join-Path $ProjectRoot 'stm32h723_app\App\Inc') `
        (Join-Path $PSScriptRoot 'test_stm32h723_speed_profile.c') `
        (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_speed_profile.c') `
        -o $output -lm
    if ($LASTEXITCODE -ne 0) { throw 'Speed profile test compilation failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'Speed profile tests failed.' }
    Write-Output 'STM32H723 speed profile tests passed.'
} finally {
    if (Test-Path $output) { Remove-Item $output -Force }
}
