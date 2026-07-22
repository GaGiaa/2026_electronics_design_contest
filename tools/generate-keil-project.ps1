[CmdletBinding()]
param(
    [string] $ProjectRoot,
    [string] $SdkRoot,
    [string] $SysConfigRoot,
    [string] $CcsRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

. (Join-Path $PSScriptRoot 'toolchain.ps1')
$toolchain = Get-ToolchainConfig -SdkRoot $SdkRoot -SysConfigRoot $SysConfigRoot -CcsRoot $CcsRoot
Assert-ToolchainConfig -Config $toolchain -Required @('SdkRoot', 'SysConfigRoot')

$projectDir = Join-Path $ProjectRoot 'keil\mspm0g3507_app'
$template = Join-Path $projectDir 'mspm0g3507_app.uvprojx'
$generated = Join-Path $projectDir 'mspm0g3507_app.local.uvprojx'

if (-not (Test-Path -LiteralPath $template -PathType Leaf)) {
    throw "Keil project template is missing: $template"
}

$content = Get-Content -Raw -Encoding UTF8 -LiteralPath $template
if (-not $content.Contains('@MSPM0_SDK_ROOT@')) {
    throw "Keil project template does not contain the @MSPM0_SDK_ROOT@ marker: $template"
}

$sdkForKeil = $toolchain.SdkRoot.TrimEnd('\')
$sdkForKeilXml = [System.Security.SecurityElement]::Escape($sdkForKeil)
$content = $content.Replace('@MSPM0_SDK_ROOT@', $sdkForKeilXml)
[xml] $null = $content
[System.IO.File]::WriteAllText($generated, $content, (New-Object System.Text.UTF8Encoding($false)))

Write-Output $generated
