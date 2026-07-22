[CmdletBinding()]
param(
    [string] $ProjectRoot,
    [string] $SdkRoot,
    [string] $SysConfigRoot,
    [string] $Compiler,
    [string] $CcsRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

. (Join-Path $PSScriptRoot 'toolchain.ps1')
$toolchain = Get-ToolchainConfig -SdkRoot $SdkRoot -SysConfigRoot $SysConfigRoot -Compiler $Compiler -CcsRoot $CcsRoot
Assert-ToolchainConfig -Config $toolchain -Required @('SdkRoot', 'SysConfig')
$SdkRoot = $toolchain.SdkRoot
$SysConfigRoot = $toolchain.SysConfigRoot
$sysConfig = $toolchain.SysConfig
$product = Join-Path $SdkRoot '.metadata\product.json'
$source = Join-Path $ProjectRoot 'mspm0g3507_app\mspm0g3507_app.syscfg'
$output = Join-Path $ProjectRoot 'keil\mspm0g3507_app\Generated'

foreach ($path in @($sysConfig, $product, $source)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required SysConfig path is unavailable: $path"
    }
}

New-Item -ItemType Directory -Force -Path $output | Out-Null
& $sysConfig --script $source --output $output --product $product --compiler keil
if ($LASTEXITCODE -ne 0) {
    throw "Keil SysConfig generation failed (exit code $LASTEXITCODE)."
}

foreach ($path in @(
        (Join-Path $output 'ti_msp_dl_config.c'),
        (Join-Path $output 'ti_msp_dl_config.h'))) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "SysConfig did not generate required file: $path"
    }
}

Write-Output "Generated Keil SysConfig files in $output"
