[CmdletBinding()]
param(
    [string] $ProjectRoot,
    [string] $KeilRoot = 'D:\Keil_v5'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

$projectFile = Join-Path $ProjectRoot 'keil\mspm0g3507_app\mspm0g3507_app.uvprojx'
$uv4 = Join-Path $KeilRoot 'UV4\UV4.exe'

foreach ($path in @($uv4, $projectFile)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required Keil open path is unavailable: $path"
    }
}

Start-Process -FilePath $uv4 -ArgumentList @($projectFile) | Out-Null
Write-Output "Opened Keil project: $projectFile"
