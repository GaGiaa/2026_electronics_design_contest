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
$testSource = Join-Path $PSScriptRoot 'test_imu_fusion.c'
$fusionSource = Join-Path $includeRoot 'algorithms\imu_fusion\imu_fusion.c'
$testOutput = Join-Path $env:TEMP 'test_imu_fusion.exe'

try {
    & $gcc '-std=c11' '-Wall' '-Wextra' '-Werror' "-I$includeRoot" `
        $testSource $fusionSource '-lm' '-o' $testOutput
    if ($LASTEXITCODE -ne 0) {
        throw "GCC IMU fusion test build failed (exit code $LASTEXITCODE)."
    }
    & $testOutput
    if ($LASTEXITCODE -ne 0) {
        throw "IMU fusion test failed (exit code $LASTEXITCODE)."
    }
}
finally {
    if (Test-Path -LiteralPath $testOutput) {
        Remove-Item -LiteralPath $testOutput -Force
    }
}

Write-Output 'MSPM0G3507 IMU fusion tests passed.'
