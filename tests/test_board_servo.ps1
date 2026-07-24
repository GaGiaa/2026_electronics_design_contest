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
$testSource = Join-Path $PSScriptRoot 'test_board_servo.c'
$appDir = Join-Path $ProjectRoot 'mspm0g3507_app'
$defaultOutput = Join-Path $env:TEMP 'test_board_servo_default.exe'
$wideRangeOutput = Join-Path $env:TEMP 'test_board_servo_270.exe'

function Invoke-ServoTest {
    param(
        [string] $Output,
        [string[]] $ExtraArguments
    )

    $arguments = @('-std=c11', '-Wall', '-Wextra', '-Werror', "-I$appDir") + $ExtraArguments + @($testSource, '-o', $Output)
    & $gcc @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Servo mapping test build failed (exit code $LASTEXITCODE)."
    }
    & $Output
    if ($LASTEXITCODE -ne 0) {
        throw "Servo mapping test failed (exit code $LASTEXITCODE)."
    }
}

try {
    Invoke-ServoTest -Output $defaultOutput -ExtraArguments @()
    Invoke-ServoTest -Output $wideRangeOutput -ExtraArguments @('-DSERVO_MAX_ANGLE_DEG=270U')
}
finally {
    Remove-Item -LiteralPath $defaultOutput, $wideRangeOutput -Force -ErrorAction SilentlyContinue
}

Write-Output 'MSPM0G3507 servo angle mapping tests passed.'
