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
$testSource = Join-Path $PSScriptRoot 'test_line_control.c'
$lineControlSource = Join-Path $includeRoot 'algorithms\line_control\line_control.c'
$pidSource = Join-Path $ProjectRoot 'shared\pid\pid.c'
$output = Join-Path $env:TEMP 'mspm0g3507_line_control_test.exe'

try {
    & $gcc '-std=c99' '-Wall' '-Wextra' '-Werror' "-I$includeRoot" "-I$(Join-Path $ProjectRoot 'shared\pid')" `
        "-I$(Join-Path $includeRoot 'algorithms\line_control')" `
        $testSource $lineControlSource $pidSource '-lm' '-o' $output
    if ($LASTEXITCODE -ne 0) {
        throw "Line control unit test build failed (exit code $LASTEXITCODE)."
    }

    & $output
    if ($LASTEXITCODE -ne 0) {
        throw "Line control unit test execution failed (exit code $LASTEXITCODE)."
    }
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}

Write-Output 'Line control unit tests passed.'
