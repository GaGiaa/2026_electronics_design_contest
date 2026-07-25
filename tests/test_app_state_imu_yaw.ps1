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
$testSource = Join-Path $PSScriptRoot 'test_app_state_imu_yaw.c'
$noImuTestSource = Join-Path $PSScriptRoot 'test_app_state_no_imu.c'
$stateSource = Join-Path $includeRoot 'app\app_state.c'
$testOutput = Join-Path $env:TEMP 'test_app_state_imu_yaw.exe'
$noImuTestOutput = Join-Path $env:TEMP 'test_app_state_no_imu.exe'

try {
    & $gcc '-std=c11' '-Wall' '-Wextra' '-Werror' `
        '-DAPP_IMU_YAW_ENABLE=1U' '-DCRSF_REMOTE_CONTROL_ENABLE=0U' `
        "-I$includeRoot" $testSource $stateSource '-lm' '-o' $testOutput
    if ($LASTEXITCODE -ne 0) {
        throw "GCC IMU yaw app-state test build failed (exit code $LASTEXITCODE)."
    }
    & $testOutput
    if ($LASTEXITCODE -ne 0) {
        throw "IMU yaw app-state test failed (exit code $LASTEXITCODE)."
    }

    & $gcc '-std=c11' '-Wall' '-Wextra' '-Werror' `
        '-DAPP_IMU_YAW_ENABLE=0U' '-DCRSF_REMOTE_CONTROL_ENABLE=0U' `
        "-I$includeRoot" $noImuTestSource $stateSource '-lm' '-o' $noImuTestOutput
    if ($LASTEXITCODE -ne 0) {
        throw "GCC no-IMU app-state test build failed (exit code $LASTEXITCODE)."
    }
    & $noImuTestOutput
    if ($LASTEXITCODE -ne 0) {
        throw "No-IMU app-state test failed (exit code $LASTEXITCODE)."
    }
}
finally {
    Remove-Item -LiteralPath $testOutput -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $noImuTestOutput -Force -ErrorAction SilentlyContinue
}

Write-Output 'IMU yaw app-state tests passed.'
