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
$testSource = Join-Path $PSScriptRoot 'test_vofa_justfloat.c'
$output = Join-Path $env:TEMP 'mspm0g3507_vofa_justfloat_test.exe'

try {
    & $gcc '-std=c99' '-Wall' '-Wextra' '-Werror' "-I$appDir" `
        (Join-Path $appDir 'vofa_justfloat.c') $testSource '-o' $output
    if ($LASTEXITCODE -ne 0) {
        throw "VOFA JustFloat test build failed (exit code $LASTEXITCODE)."
    }

    & $output
    if ($LASTEXITCODE -ne 0) {
        throw "VOFA JustFloat test execution failed (exit code $LASTEXITCODE)."
    }

    Write-Output 'VOFA JustFloat unit tests passed.'
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}
