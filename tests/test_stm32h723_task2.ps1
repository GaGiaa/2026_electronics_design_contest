param([string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$compiler = Get-Command gcc -ErrorAction Stop
$output = Join-Path $env:TEMP 'test_stm32h723_task2.exe'
& $compiler.Source -std=c11 -Wall -Wextra -Werror `
    -I (Join-Path $ProjectRoot 'stm32h723_app\App\Inc') `
    (Join-Path $PSScriptRoot 'test_stm32h723_task2.c') `
    (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_task2.c') `
    -o $output
if ($LASTEXITCODE -ne 0) { throw 'Task 2 test compilation failed.' }
& $output
if ($LASTEXITCODE -ne 0) { throw 'Task 2 tests failed.' }
Write-Output 'STM32H723 task 2 tests passed.'
