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
$appDir = Join-Path $ProjectRoot 'mspm0g3507_app'
$testSource = Join-Path $PSScriptRoot 'test_motor_control.c'
$output = Join-Path $env:TEMP 'mspm0g3507_motor_control_test.exe'

try {
    & $gcc '-std=c99' '-Wall' '-Wextra' '-Werror' "-I$appDir" "-I$appDir\motor_pid" `
        (Join-Path $appDir 'motor_control.c') `
        (Join-Path $appDir 'motor_pid\pid.c') `
        (Join-Path $appDir 'vofa_justfloat.c') `
        $testSource '-lm' '-o' $output
    if ($LASTEXITCODE -ne 0) {
        throw "Motor control test build failed (exit code $LASTEXITCODE)."
    }

    & $output
    if ($LASTEXITCODE -ne 0) {
        throw "Motor control test execution failed (exit code $LASTEXITCODE)."
    }
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}
