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
$appDir = Join-Path $ProjectRoot 'stm32h723_app'
$testSource = Join-Path $PSScriptRoot 'test_stm32h723_buttons.c'
$buttonSource = Join-Path $appDir 'App\Src\app_buttons.c'
$output = Join-Path $env:TEMP 'stm32h723_buttons_test.exe'

try {
    & $gcc '-std=c99' '-Wall' '-Wextra' '-Werror' `
        "-I$($PSScriptRoot)\stubs" "-I$($appDir)\App\Inc" `
        $buttonSource $testSource '-o' $output
    if ($LASTEXITCODE -ne 0) {
        throw "STM32H723 button test build failed (exit code $LASTEXITCODE)."
    }

    & $output
    if ($LASTEXITCODE -ne 0) {
        throw "STM32H723 button test execution failed (exit code $LASTEXITCODE)."
    }

    Write-Output 'STM32H723 button tests passed.'
}
finally {
    Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
}
