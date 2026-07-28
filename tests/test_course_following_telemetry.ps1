[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$appDir = Join-Path $ProjectRoot 'mspm0g3507_app'
$output = Join-Path $env:TEMP 'mspm0g3507_course_telemetry_test.exe'

try {
    & $gcc '-std=c11' '-Wall' '-Wextra' '-Werror' "-I$appDir" `
        (Join-Path $appDir 'protocols\vofa\course_following_telemetry.c') `
        (Join-Path $appDir 'protocols\vofa\vofa_justfloat.c') `
        (Join-Path $PSScriptRoot 'test_course_following_telemetry.c') `
        '-o' $output
    if ($LASTEXITCODE -ne 0) { throw "Course telemetry test build failed (exit code $LASTEXITCODE)." }
    & $output
    if ($LASTEXITCODE -ne 0) { throw "Course telemetry test failed (exit code $LASTEXITCODE)." }
    Write-Output 'Course following telemetry tests passed.'
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}
