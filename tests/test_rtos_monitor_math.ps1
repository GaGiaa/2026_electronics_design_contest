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
$testSource = Join-Path $ProjectRoot 'tests\test_rtos_monitor_math.c'
$monitorSource = Join-Path $ProjectRoot 'mspm0g3507_app\services\rtos_monitor\rtos_monitor.c'
$testOutput = Join-Path $env:TEMP 'test_rtos_monitor_math.exe'

try {
    & $gcc '-std=c11' '-Wall' '-Wextra' '-Werror' '-DRTOS_MONITOR_HOST_TEST' `
        "-I$(Join-Path $ProjectRoot 'mspm0g3507_app')" $testSource $monitorSource `
        '-o' $testOutput
    if ($LASTEXITCODE -ne 0) {
        throw "GCC RTOS monitor math test build failed (exit code $LASTEXITCODE)."
    }
    & $testOutput
    if ($LASTEXITCODE -ne 0) {
        throw "RTOS monitor math test failed (exit code $LASTEXITCODE)."
    }
}
finally {
    if (Test-Path -LiteralPath $testOutput) {
        Remove-Item -LiteralPath $testOutput -Force
    }
}

Write-Output 'RTOS monitor math tests passed.'
