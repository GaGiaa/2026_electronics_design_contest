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
$testSource = Join-Path $PSScriptRoot 'test_host_link.c'
$implementation = Join-Path $appDir 'protocols\host_link\host_link.c'
$output = Join-Path $env:TEMP 'mspm0g3507_host_link_test.exe'

try {
    & $gcc '-std=c99' '-Wall' '-Wextra' '-Werror' "-I$appDir" $implementation $testSource '-o' $output
    if ($LASTEXITCODE -ne 0) {
        throw "Host link unit test build failed (exit code $LASTEXITCODE)."
    }

    & $output
    if ($LASTEXITCODE -ne 0) {
        throw "Host link unit test execution failed (exit code $LASTEXITCODE)."
    }

    Write-Output 'Host link unit tests passed.'
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}
