[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$appDir = Join-Path $ProjectRoot 'mspm0g3507_app'
$output = Join-Path $env:TEMP 'mspm0g3507_course_following_test.exe'

try {
    & $gcc '-std=c11' '-Wall' '-Wextra' '-Werror' "-I$appDir" `
        (Join-Path $appDir 'algorithms\course_following\course_following.c') `
        (Join-Path $appDir 'algorithms\pid\pid.c') `
        (Join-Path $PSScriptRoot 'test_course_following.c') '-lm' '-o' $output
    if ($LASTEXITCODE -ne 0) { throw "Course following test build failed (exit code $LASTEXITCODE)." }
    & $output
    if ($LASTEXITCODE -ne 0) { throw "Course following test failed (exit code $LASTEXITCODE)." }
    Write-Output 'Course following unit tests passed.'
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}
