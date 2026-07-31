[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

$compiler = Get-Command gcc -ErrorAction Stop
$output = Join-Path $env:TEMP 'test_stm32h723_task56.exe'
$includeRoot = Join-Path $ProjectRoot 'stm32h723_app\App\Inc'
$testSource = Join-Path $PSScriptRoot 'test_stm32h723_task56.c'
$taskSource = Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_task56.c'

try {
    & $compiler.Source '-std=c11' '-Wall' '-Wextra' '-Werror' `
        "-I$includeRoot" $testSource $taskSource '-lm' '-o' $output
    if ($LASTEXITCODE -ne 0) {
        throw 'STM32H723 task 5/6 test build failed.'
    }
    & $output
    if ($LASTEXITCODE -ne 0) {
        throw 'STM32H723 task 5/6 tests failed.'
    }
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}

Write-Output 'STM32H723 task 5/6 unit tests passed.'
