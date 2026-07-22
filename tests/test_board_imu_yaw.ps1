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
$testSource = Join-Path $ProjectRoot 'tests\test_board_imu_yaw.c'
$yawSource = Join-Path $ProjectRoot 'mspm0g3507_app\board_imu_yaw.c'
$testOutput = Join-Path $env:TEMP 'test_board_imu_yaw.exe'

try {
    & $gcc '-std=c11' '-Wall' '-Wextra' '-Werror' "-I$(Join-Path $ProjectRoot 'mspm0g3507_app')" $testSource $yawSource '-lm' '-o' $testOutput
    if ($LASTEXITCODE -ne 0) {
        throw "GCC yaw test build failed (exit code $LASTEXITCODE)."
    }
    & $testOutput
    if ($LASTEXITCODE -ne 0) {
        throw "Yaw algorithm test failed (exit code $LASTEXITCODE)."
    }
}
finally {
    if (Test-Path -LiteralPath $testOutput) {
        Remove-Item -LiteralPath $testOutput -Force
    }
}

Write-Output 'MSPM0G3507 IMU yaw algorithm tests passed.'
