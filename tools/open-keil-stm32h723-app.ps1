[CmdletBinding(SupportsShouldProcess)]
param(
    [string] $ProjectRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

$projectFile = Join-Path $ProjectRoot 'stm32h723_app\MDK-ARM\stm32h723_app.uvprojx'
$fallbackUv4 = 'D:\Keil_v5\UV4\UV4.exe'
$configuredKeilRoot = $env:KEIL_ROOT
$uv4 = $null

if (-not [string]::IsNullOrWhiteSpace($configuredKeilRoot)) {
    $configuredUv4 = Join-Path $configuredKeilRoot 'UV4\UV4.exe'
    if (Test-Path -LiteralPath $configuredUv4 -PathType Leaf) {
        $uv4 = $configuredUv4
    }
}

if ([string]::IsNullOrWhiteSpace($uv4) -and (Test-Path -LiteralPath $fallbackUv4 -PathType Leaf)) {
    $uv4 = $fallbackUv4
}

if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw "STM32H723 Keil project is unavailable: $projectFile"
}

if ([string]::IsNullOrWhiteSpace($uv4)) {
    throw "Keil UV4.exe was not found. Set KEIL_ROOT to a Keil_v5 directory or install Keil at $fallbackUv4"
}

if ($PSCmdlet.ShouldProcess($projectFile, "Open with $uv4")) {
    Start-Process -FilePath $uv4 -ArgumentList @($projectFile) -WorkingDirectory (Split-Path -Parent $projectFile) | Out-Null
}

Write-Output "Keil executable: $uv4"
Write-Output "Keil project: $projectFile"
