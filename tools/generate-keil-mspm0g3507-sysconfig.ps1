[CmdletBinding()]
param(
    [string] $ProjectRoot,
    [string] $SdkRoot = 'D:\Software\ti\ccs2020\mspm0_sdk_2_11_00_07',
    [string] $SysConfigRoot = 'D:\Software\ti\ccs2020\sysconfig_1.26.2'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

$sysConfig = Join-Path $SysConfigRoot 'sysconfig_cli.bat'
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
