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
$testSource = Join-Path $ProjectRoot 'tests\test_ultrasonic_measurement.c'
$measurementSource = Join-Path $ProjectRoot 'mspm0g3507_app\algorithms\ultrasonic\ultrasonic_measurement.c'
$testOutput = Join-Path $env:TEMP 'test_ultrasonic_measurement.exe'

try {
    & $gcc '-std=c11' '-Wall' '-Wextra' '-Werror' `
        "-I$(Join-Path $ProjectRoot 'mspm0g3507_app')" $testSource `
        $measurementSource '-o' $testOutput
    if ($LASTEXITCODE -ne 0) {
        throw "GCC ultrasonic measurement test build failed (exit code $LASTEXITCODE)."
    }
    & $testOutput
    if ($LASTEXITCODE -ne 0) {
        throw "Ultrasonic measurement test failed (exit code $LASTEXITCODE)."
    }
}
finally {
    if (Test-Path -LiteralPath $testOutput) {
        Remove-Item -LiteralPath $testOutput -Force
    }
}

Write-Output 'Ultrasonic measurement tests passed.'
