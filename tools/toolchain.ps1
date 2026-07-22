[CmdletBinding()]
param()

Set-StrictMode -Version Latest

function ConvertTo-AbsoluteToolPath {
    param(
        [Parameter(Mandatory)] [string] $Path
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path)
    )
}

function Resolve-ToolPath {
    param(
        [string] $ExplicitPath,
        [string] $EnvironmentVariable,
        [string[]] $Candidates = @()
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        return ConvertTo-AbsoluteToolPath $ExplicitPath
    }

    if (-not [string]::IsNullOrWhiteSpace($EnvironmentVariable)) {
        return ConvertTo-AbsoluteToolPath $EnvironmentVariable
    }

    foreach ($candidate in $Candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate)) {
            return ConvertTo-AbsoluteToolPath $candidate
        }
    }

    return $null
}

function Get-NewestChildDirectory {
    param(
        [Parameter(Mandatory)] [string] $Parent,
        [Parameter(Mandatory)] [string] $Filter
    )

    if (-not (Test-Path -LiteralPath $Parent -PathType Container)) {
        return $null
    }

    $child = Get-ChildItem -LiteralPath $Parent -Directory -Filter $Filter -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        Select-Object -First 1
    if ($null -eq $child) {
        return $null
    }

    return $child.FullName
}

function Get-ToolchainSearchParents {
    $parents = @()
    foreach ($drive in Get-PSDrive -PSProvider FileSystem) {
        foreach ($relativePath in @('Software\ti', 'ti', 'Texas Instruments', 'Software')) {
            $parent = Join-Path $drive.Root $relativePath
            if (Test-Path -LiteralPath $parent -PathType Container) {
                $parents += $parent
            }
        }
    }

    foreach ($programFiles in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
        if (-not [string]::IsNullOrWhiteSpace($programFiles)) {
            foreach ($relativePath in @('Texas Instruments', 'Keil_v5')) {
                $parent = Join-Path $programFiles $relativePath
                if (Test-Path -LiteralPath $parent -PathType Container) {
                    $parents += $parent
                }
            }
        }
    }

    return $parents | Select-Object -Unique
}

function Find-InstalledCcsRoot {
    foreach ($parent in Get-ToolchainSearchParents) {
        $candidates = Get-ChildItem -LiteralPath $parent -Directory -Filter 'ccs*' -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending
        foreach ($candidate in $candidates) {
            if ((Test-Path -LiteralPath (Join-Path $candidate.FullName 'ccs\utils\bin\gmake.exe')) -and
                (Test-Path -LiteralPath (Join-Path $candidate.FullName 'ccs\tools\compiler'))) {
                return $candidate.FullName
            }
        }
    }

    return $null
}

function Find-InstalledKeilRoot {
    foreach ($drive in Get-PSDrive -PSProvider FileSystem) {
        $candidate = Join-Path $drive.Root 'Keil_v5'
        if (Test-Path -LiteralPath (Join-Path $candidate 'UV4\UV4.exe')) {
            return $candidate
        }
    }

    foreach ($programFiles in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
        if (-not [string]::IsNullOrWhiteSpace($programFiles)) {
            $candidate = Join-Path $programFiles 'Keil_v5'
            if (Test-Path -LiteralPath (Join-Path $candidate 'UV4\UV4.exe')) {
                return $candidate
            }
        }
    }

    return $null
}

function Find-InstalledGdbPath {
    $fromPath = Get-Command 'arm-none-eabi-gdb.exe' -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty Source -First 1
    if (-not [string]::IsNullOrWhiteSpace($fromPath)) {
        return $fromPath
    }

    foreach ($parent in Get-ToolchainSearchParents) {
        $candidates = Get-ChildItem -LiteralPath $parent -Directory -Filter 'STM32CubeCLT*' -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending
        foreach ($candidate in $candidates) {
            $gdb = Join-Path $candidate.FullName 'GNU-tools-for-STM32\bin\arm-none-eabi-gdb.exe'
            if (Test-Path -LiteralPath $gdb -PathType Leaf) {
                return $gdb
            }
        }
    }

    return $null
}

function Get-ToolchainConfig {
    [CmdletBinding()]
    param(
        [string] $SdkRoot,
        [string] $SysConfigRoot,
        [string] $Compiler,
        [string] $CcsRoot,
        [string] $KeilRoot,
        [string] $GdbPath
    )

    $ccsRoot = Resolve-ToolPath -ExplicitPath $CcsRoot -EnvironmentVariable $env:CCS_ROOT -Candidates @(
        (Join-Path ${env:ProgramFiles} 'Texas Instruments\Code Composer Studio'),
        (Join-Path ${env:ProgramFiles(x86)} 'Texas Instruments\Code Composer Studio'),
        (Find-InstalledCcsRoot)
    )

    $sdkCandidates = @()
    $sysConfigCandidates = @()
    $compilerCandidates = @()
    if (-not [string]::IsNullOrWhiteSpace($ccsRoot)) {
        $ccsParent = Split-Path -Parent $ccsRoot
        $sdkCandidate = Get-NewestChildDirectory -Parent $ccsRoot -Filter 'mspm0_sdk_*'
        if ($null -eq $sdkCandidate) {
            $sdkCandidate = Get-NewestChildDirectory -Parent $ccsParent -Filter 'mspm0_sdk_*'
        }
        if ($null -ne $sdkCandidate) { $sdkCandidates += $sdkCandidate }

        $sysConfigCandidate = Get-NewestChildDirectory -Parent $ccsRoot -Filter 'sysconfig_*'
        if ($null -eq $sysConfigCandidate) {
            $sysConfigCandidate = Get-NewestChildDirectory -Parent $ccsParent -Filter 'sysconfig_*'
        }
        if ($null -ne $sysConfigCandidate) { $sysConfigCandidates += $sysConfigCandidate }

        $compilerCandidate = Get-ChildItem -LiteralPath (Join-Path $ccsRoot 'ccs\tools\compiler') -File -Filter 'tiarmclang.exe' -Recurse -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending |
            Select-Object -First 1
        if ($null -ne $compilerCandidate) { $compilerCandidates += $compilerCandidate.FullName }
    }

    $sdkRoot = Resolve-ToolPath -ExplicitPath $SdkRoot -EnvironmentVariable $env:MSPM0_SDK_ROOT -Candidates $sdkCandidates
    $sysConfigRoot = Resolve-ToolPath -ExplicitPath $SysConfigRoot -EnvironmentVariable $env:SYSCONFIG_ROOT -Candidates $sysConfigCandidates
    $compiler = Resolve-ToolPath -ExplicitPath $Compiler -EnvironmentVariable $env:TI_ARM_CLANG -Candidates $compilerCandidates
    $keilRoot = Resolve-ToolPath -ExplicitPath $KeilRoot -EnvironmentVariable $env:KEIL_ROOT -Candidates @(
        (Join-Path ${env:ProgramFiles} 'Keil_v5'),
        (Join-Path ${env:ProgramFiles(x86)} 'Keil_v5'),
        (Find-InstalledKeilRoot)
    )
    $gdbPath = Resolve-ToolPath -ExplicitPath $GdbPath -EnvironmentVariable $env:ARM_GDB_PATH -Candidates @(
        (Find-InstalledGdbPath)
    )

    $gmake = $null
    if (-not [string]::IsNullOrWhiteSpace($ccsRoot)) {
        $gmake = Resolve-ToolPath -EnvironmentVariable $null -Candidates @(
            (Join-Path $ccsRoot 'ccs\utils\bin\gmake.exe')
        )
    }

    $sysConfigCli = $null
    if (-not [string]::IsNullOrWhiteSpace($sysConfigRoot)) {
        $sysConfigCli = Join-Path $sysConfigRoot 'sysconfig_cli.bat'
    }

    $uv4 = $null
    if (-not [string]::IsNullOrWhiteSpace($keilRoot)) {
        $uv4 = Join-Path $keilRoot 'UV4\UV4.exe'
    }

    [pscustomobject]@{
        CcsRoot = $ccsRoot
        SdkRoot = $sdkRoot
        SysConfigRoot = $sysConfigRoot
        SysConfig = $sysConfigCli
        Compiler = $compiler
        Gmake = $gmake
        KeilRoot = $keilRoot
        Uv4 = $uv4
        GdbPath = $gdbPath
    }
}

function Assert-ToolchainConfig {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)] [psobject] $Config,
        [Parameter(Mandatory)] [string[]] $Required
    )

    foreach ($name in $Required) {
        $value = $Config.$name
        if ([string]::IsNullOrWhiteSpace([string] $value) -or -not (Test-Path -LiteralPath $value)) {
            $envName = switch ($name) {
                'SdkRoot' { 'MSPM0_SDK_ROOT' }
                'SysConfigRoot' { 'SYSCONFIG_ROOT' }
                'Compiler' { 'TI_ARM_CLANG' }
                'CcsRoot' { 'CCS_ROOT' }
                'KeilRoot' { 'KEIL_ROOT' }
                'GdbPath' { 'ARM_GDB_PATH' }
                'SysConfig' { 'SYSCONFIG_ROOT' }
                'Gmake' { 'CCS_ROOT' }
                'Uv4' { 'KEIL_ROOT' }
                default { $name }
            }
            throw "Required toolchain path '$name' is unavailable: '$value'. Set $envName or pass the corresponding script parameter."
        }
    }
}
