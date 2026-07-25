[CmdletBinding()]
param(
    [string] $ProjectRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$includeRoot = Join-Path $ProjectRoot 'mspm0g3507_app'
$supportRoot = Join-Path $ProjectRoot 'tests\test_support'
$testSource = Join-Path $PSScriptRoot 'test_board_grayscale.c'
$driverSource = Join-Path $includeRoot 'drivers\grayscale\board_grayscale.c'
$output = Join-Path $env:TEMP 'test_board_grayscale.exe'

try {
    & $gcc '-std=c11' '-Wall' '-Wextra' '-Werror' `
        "-I$supportRoot" "-I$includeRoot" $testSource $driverSource '-o' $output
    if ($LASTEXITCODE -ne 0) {
        throw "Grayscale driver test build failed (exit code $LASTEXITCODE)."
    }

    & $output
    if ($LASTEXITCODE -ne 0) {
        throw "Grayscale driver test failed (exit code $LASTEXITCODE)."
    }
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}

Write-Output 'Grayscale driver unit tests passed.'
