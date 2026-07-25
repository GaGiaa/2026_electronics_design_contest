[CmdletBinding()]
param(
    [string] $ProjectRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

function Assert-Equal {
    param(
        [Parameter(Mandatory)] $Actual,
        [Parameter(Mandatory)] $Expected,
        [Parameter(Mandatory)] [string] $Description
    )

    if ($Actual -ne $Expected) {
        throw "${Description}: expected '$Expected', got '$Actual'."
    }
}

$toolchainScript = Join-Path $ProjectRoot 'tools\toolchain.ps1'
if (-not (Test-Path -LiteralPath $toolchainScript)) {
    throw "Toolchain resolver is missing: $toolchainScript"
}
. $toolchainScript

$configureScript = Join-Path $ProjectRoot 'tools\configure-toolchain.ps1'
if (-not (Test-Path -LiteralPath $configureScript)) {
    throw "Toolchain configuration script is missing: $configureScript"
}
if (-not (Select-String -LiteralPath $configureScript -Pattern 'PersistUserEnvironment' -Quiet)) {
    throw 'Toolchain configuration script must support persistent user environment setup.'
}

$trackedFiles = & git -C $ProjectRoot ls-files
if ($LASTEXITCODE -ne 0) {
    throw 'Could not enumerate tracked files.'
}

$driveD = 'D' + ':\'
$driveCUsers = 'C' + ':\Users\'
$forbiddenPathPattern = '(?i)(' +
    [regex]::Escape($driveD + 'Software') + '|' +
    [regex]::Escape($driveD + 'Keil_v5') + '|' +
    [regex]::Escape($driveD + 'desktop') + '|' +
    [regex]::Escape($driveCUsers) + ')'
foreach ($relativePath in $trackedFiles) {
    $path = Join-Path $ProjectRoot ($relativePath -replace '/', '\\')
    if ((Test-Path -LiteralPath $path) -and (Select-String -LiteralPath $path -Pattern $forbiddenPathPattern -Quiet)) {
        throw "Machine-specific absolute path found in tracked file: $relativePath"
    }
}

$sdkFromEnvironment = Join-Path $env:TEMP 'portable-sdk-from-environment'
$sdkExplicit = Join-Path $env:TEMP 'portable-sdk-explicit'
$env:MSPM0_SDK_ROOT = $sdkFromEnvironment
$env:SYSCONFIG_ROOT = Join-Path $env:TEMP 'portable-sysconfig'
$env:TI_ARM_CLANG = Join-Path $env:TEMP 'portable-tiarmclang.exe'
$env:CCS_ROOT = Join-Path $env:TEMP 'portable-ccs'
$env:KEIL_ROOT = Join-Path $env:TEMP 'portable-keil'
$env:ARM_GDB_PATH = Join-Path $env:TEMP 'portable-gdb.exe'

$fromEnvironment = Get-ToolchainConfig
Assert-Equal -Actual $fromEnvironment.SdkRoot -Expected $sdkFromEnvironment -Description 'SDK environment override'
Assert-Equal -Actual $fromEnvironment.SysConfigRoot -Expected $env:SYSCONFIG_ROOT -Description 'SysConfig environment override'
Assert-Equal -Actual $fromEnvironment.Compiler -Expected $env:TI_ARM_CLANG -Description 'compiler environment override'
Assert-Equal -Actual $fromEnvironment.KeilRoot -Expected $env:KEIL_ROOT -Description 'Keil environment override'
Assert-Equal -Actual $fromEnvironment.GdbPath -Expected $env:ARM_GDB_PATH -Description 'GDB environment override'

$explicit = Get-ToolchainConfig -SdkRoot $sdkExplicit
Assert-Equal -Actual $explicit.SdkRoot -Expected $sdkExplicit -Description 'explicit SDK override'
Assert-Equal -Actual $explicit.SysConfigRoot -Expected $env:SYSCONFIG_ROOT -Description 'environment fallback after explicit override'

$candidateCcsRoots = @()
foreach ($drive in Get-PSDrive -PSProvider FileSystem) {
    foreach ($parent in @(
            (Join-Path $drive.Root 'Software\ti'),
            (Join-Path $drive.Root 'ti'),
            (Join-Path $drive.Root 'Texas Instruments'))) {
        $candidateCcsRoots += Get-ChildItem -LiteralPath $parent -Directory -Filter 'ccs*' -ErrorAction SilentlyContinue
    }
}
$installedCcs = $candidateCcsRoots |
    Where-Object {
        (Test-Path -LiteralPath (Join-Path $_.FullName 'ccs\utils\bin\gmake.exe')) -and
        (Test-Path -LiteralPath (Join-Path $_.FullName 'ccs\tools\compiler'))
    } |
    Select-Object -First 1
if ($null -ne $installedCcs) {
    foreach ($name in @('MSPM0_SDK_ROOT', 'SYSCONFIG_ROOT', 'TI_ARM_CLANG', 'CCS_ROOT', 'KEIL_ROOT', 'ARM_GDB_PATH')) {
        Remove-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue
    }
    $autoDiscovered = Get-ToolchainConfig
    Assert-Equal -Actual $autoDiscovered.CcsRoot -Expected $installedCcs.FullName -Description 'automatic CCS discovery'
    Assert-Equal -Actual $autoDiscovered.Gmake -Expected (Join-Path $installedCcs.FullName 'ccs\utils\bin\gmake.exe') -Description 'automatic gmake discovery'
}

Write-Host 'Portable toolchain and path checks passed.'
