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
$testSource = Join-Path $PSScriptRoot 'test_yaw_control.c'
$yawSource = Join-Path $includeRoot 'algorithms\yaw_control\yaw_control.c'
$pidSource = Join-Path $ProjectRoot 'shared\pid\pid.c'
$output = Join-Path $env:TEMP 'mspm0g3507_yaw_control_test.exe'

try {
    & $gcc '-std=c99' '-Wall' '-Wextra' '-Werror' "-I$includeRoot" "-I$(Join-Path $ProjectRoot 'shared\pid')" `
        "-I$(Join-Path $includeRoot 'algorithms\yaw_control')" `
        $testSource $yawSource $pidSource '-lm' '-o' $output
    if ($LASTEXITCODE -ne 0) {
        throw "Yaw control unit test build failed (exit code $LASTEXITCODE)."
    }

    & $output
    if ($LASTEXITCODE -ne 0) {
        throw "Yaw control unit test execution failed (exit code $LASTEXITCODE)."
    }
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}

Write-Output 'Yaw control unit tests passed.'
