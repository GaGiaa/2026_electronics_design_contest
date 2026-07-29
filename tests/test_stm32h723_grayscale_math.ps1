[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$includeRoot = Join-Path $ProjectRoot 'stm32h723_app\App\Inc'
$testSource = Join-Path $PSScriptRoot 'test_stm32h723_grayscale_math.c'
$mathSource = Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_grayscale_math.c'
$output = Join-Path $env:TEMP 'test_stm32h723_grayscale_math.exe'

try {
    & $gcc '-std=c11' '-Wall' '-Wextra' '-Werror' "-I$includeRoot" $testSource $mathSource '-o' $output
    if ($LASTEXITCODE -ne 0) { throw "H723 grayscale math test build failed (exit code $LASTEXITCODE)." }
    & $output
    if ($LASTEXITCODE -ne 0) { throw "H723 grayscale math test failed (exit code $LASTEXITCODE)." }
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}

Write-Output 'STM32H723 grayscale math unit tests passed.'
