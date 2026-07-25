[CmdletBinding()]
param(
    [string] $ProjectRoot,
    [ValidateSet(1, 2)]
    [int] $EncoderDecodeMode
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
$compilerArguments = @('-std=c99', '-Wall', '-Wextra', '-Werror', '-DCRSF_REMOTE_CONTROL_ENABLE=0', "-I$appDir", "-I$(Join-Path $appDir 'algorithms\pid')")
    if ($PSBoundParameters.ContainsKey('EncoderDecodeMode')) {
        $compilerArguments += "-DBOARD_ENCODER_DECODE_MODE=$EncoderDecodeMode"
    }
    & $gcc @compilerArguments `
        (Join-Path $appDir 'algorithms\motor_control\motor_control.c') `
        (Join-Path $appDir 'algorithms\encoder\encoder_quadrature.c') `
        (Join-Path $appDir 'algorithms\encoder\encoder_speed_filter.c') `
        (Join-Path $appDir 'algorithms\pid\pid.c') `
        (Join-Path $appDir 'protocols\vofa\vofa_justfloat.c') `
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
