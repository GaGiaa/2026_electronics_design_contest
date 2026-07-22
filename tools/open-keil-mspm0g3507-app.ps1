[CmdletBinding()]
param(
    [string] $ProjectRoot,
    [string] $KeilRoot,
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
$toolchain = Get-ToolchainConfig -KeilRoot $KeilRoot -SdkRoot $SdkRoot -SysConfigRoot $SysConfigRoot -CcsRoot $CcsRoot
Assert-ToolchainConfig -Config $toolchain -Required @('KeilRoot', 'Uv4', 'SdkRoot', 'SysConfigRoot')
$KeilRoot = $toolchain.KeilRoot
$uv4 = $toolchain.Uv4
$generator = Join-Path $PSScriptRoot 'generate-keil-project.ps1'
$projectFile = & $generator -ProjectRoot $ProjectRoot -SdkRoot $toolchain.SdkRoot -SysConfigRoot $toolchain.SysConfigRoot
if ([string]::IsNullOrWhiteSpace($projectFile) -or -not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw 'Keil project generation failed.'
}

foreach ($path in @($uv4, $projectFile)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required Keil open path is unavailable: $path"
    }
}

Start-Process -FilePath $uv4 -ArgumentList @($projectFile) | Out-Null
Write-Output "Opened Keil project: $projectFile"
