[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$includeRoot = Join-Path $ProjectRoot 'stm32h723_app\App\Inc'
$testSource = Join-Path $PSScriptRoot 'test_stm32h723_task_menu.c'
$menuSource = Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_task_menu.c'
$output = Join-Path $env:TEMP 'test_stm32h723_task_menu.exe'

try {
    & $gcc '-std=c11' '-Wall' '-Wextra' '-Werror' "-I$includeRoot" `
        $testSource $menuSource '-o' $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 task-menu test build failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 task-menu test failed.' }
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}

Write-Output 'STM32H723 task-menu unit tests passed.'
