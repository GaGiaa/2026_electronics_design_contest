[CmdletBinding()]
param(
    [string] $ProjectRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

function Assert-Contains {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Pattern,
        [Parameter(Mandatory)] [string] $Description
    )

    if (-not (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) {
        throw "${Description}: '$Pattern' was not found in $Path."
    }
}

function Assert-NotContains {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Pattern,
        [Parameter(Mandatory)] [string] $Description
    )

    if (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet) {
        throw "${Description}: '$Pattern' must not be present in $Path."
    }
}

$appReadme = Join-Path $ProjectRoot 'mspm0g3507_app\README.md'
$setup = Join-Path $ProjectRoot 'docs\SETUP.md'
$handoff = Join-Path $ProjectRoot 'docs\AI_HANDOFF.md'
$buildScript = Join-Path $ProjectRoot 'tools\build-mspm0g3507-app.ps1'

foreach ($path in @($appReadme, $setup, $handoff, $buildScript)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "CCS app workflow documentation is incomplete: $path is missing."
    }
}

Assert-Contains -Path $appReadme -Pattern 'tools/build-mspm0g3507-app\.ps1' -Description 'App README must name the canonical build script'
Assert-Contains -Path $appReadme -Pattern 'CCS.*Build|Build.*CCS|CCS.*编译|CCS.*构建' -Description 'App README must explain the CCS GUI build boundary'
Assert-Contains -Path $appReadme -Pattern 'FreeRTOS\.h|FreeRTOS' -Description 'App README must explain the external FreeRTOS dependency'
Assert-Contains -Path $setup -Pattern 'MSPM0 SDK 2\.11\.00\.07' -Description 'Setup guide must pin the supported SDK version'
Assert-Contains -Path $setup -Pattern 'sysconfig_1\.26\.2|SysConfig 1\.26\.2' -Description 'Setup guide must pin the supported SysConfig version'
Assert-Contains -Path $setup -Pattern 'build-mspm0g3507-app\.ps1' -Description 'Setup guide must provide the portable app build command'
Assert-Contains -Path $handoff -Pattern 'CCS.*workspace|FreeRTOS.*include|Project.*Build' -Description 'Handoff must document the CCS app build boundary'
Assert-Contains -Path $buildScript -Pattern 'FreeRtosRoot' -Description 'Build script must resolve FreeRTOS from the selected SDK'
Assert-Contains -Path $buildScript -Pattern 'motor_pid\\pid\.c' -Description 'Build script must compile the repository PID source'
Assert-NotContains -Path $appReadme -Pattern 'Build the CCS or Keil app with' -Description 'README must not imply the CCS GUI is a complete app build'

Write-Host 'PASS: CCS app workflow checks passed.'
