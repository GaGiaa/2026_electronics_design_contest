param([string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$compiler = Get-Command gcc -ErrorAction Stop
$output = Join-Path $env:TEMP 'test_stm32h723_task3.exe'
& $compiler.Source -std=c11 -Wall -Wextra -Werror `
    -I (Join-Path $ProjectRoot 'stm32h723_app\App\Inc') `
    (Join-Path $PSScriptRoot 'test_stm32h723_task3.c') `
    (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_task3.c') `
    -o $output
if ($LASTEXITCODE -ne 0) { throw 'Task 3 test compilation failed.' }
& $output
if ($LASTEXITCODE -ne 0) { throw 'Task 3 tests failed.' }
Write-Output 'STM32H723 task 3 tests passed.'
