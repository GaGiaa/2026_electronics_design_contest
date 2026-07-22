[CmdletBinding()]
param(
    [switch] $PersistUserEnvironment,
    [string] $SdkRoot,
    [string] $SysConfigRoot,
    [string] $Compiler,
    [string] $CcsRoot,
    [string] $KeilRoot,
    [string] $GdbPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'toolchain.ps1')
$config = Get-ToolchainConfig `
    -SdkRoot $SdkRoot `
    -SysConfigRoot $SysConfigRoot `
    -Compiler $Compiler `
    -CcsRoot $CcsRoot `
    -KeilRoot $KeilRoot `
    -GdbPath $GdbPath
Assert-ToolchainConfig -Config $config -Required @('SdkRoot', 'SysConfigRoot', 'Compiler', 'CcsRoot', 'KeilRoot', 'Uv4', 'GdbPath')

$environment = [ordered]@{
    MSPM0_SDK_ROOT = $config.SdkRoot
    SYSCONFIG_ROOT = $config.SysConfigRoot
    TI_ARM_CLANG = $config.Compiler
    CCS_ROOT = $config.CcsRoot
    KEIL_ROOT = $config.KeilRoot
    ARM_GDB_PATH = $config.GdbPath
}

foreach ($entry in $environment.GetEnumerator()) {
    [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value, 'Process')
    if ($PersistUserEnvironment) {
        [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value, 'User')
    }
    Write-Output "$($entry.Key)=$($entry.Value)"
}

if ($PersistUserEnvironment) {
    Write-Output 'Toolchain paths were saved to the current user environment. Restart VS Code to reload them.'
}
