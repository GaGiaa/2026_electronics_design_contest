[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$includeRoot = Join-Path $ProjectRoot 'stm32h723_app\App\Inc'
$pidInclude = Join-Path $ProjectRoot 'shared\pid'
$testSource = Join-Path $PSScriptRoot 'test_stm32h723_line_follow.c'
$lineFollowSource = Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_line_follow.c'
$pidSource = Join-Path $ProjectRoot 'shared\pid\pid.c'
$output = Join-Path $env:TEMP 'test_stm32h723_line_follow.exe'

try {
    & $gcc '-std=c11' '-Wall' '-Wextra' '-Werror' "-I$includeRoot" "-I$pidInclude" `
        $testSource $lineFollowSource $pidSource '-lm' '-o' $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 line-follow test build failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 line-follow test failed.' }
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}

Write-Output 'STM32H723 line-follow unit tests passed.'
