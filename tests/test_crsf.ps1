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
$testSource = Join-Path $PSScriptRoot 'test_crsf.c'
$output = Join-Path $env:TEMP 'mspm0g3507_crsf_test.exe'

try {
    & $gcc '-std=c99' '-Wall' '-Wextra' '-Werror' "-I$appDir" `
        (Join-Path $appDir 'crsf_protocol.c') `
        (Join-Path $appDir 'crsf_control.c') `
        $testSource '-lm' '-o' $output
    if ($LASTEXITCODE -ne 0) {
        throw "CRSF unit test build failed (exit code $LASTEXITCODE)."
    }

    & $output
    if ($LASTEXITCODE -ne 0) {
        throw "CRSF unit test execution failed (exit code $LASTEXITCODE)."
    }

    Write-Output 'CRSF unit tests passed.'
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}
