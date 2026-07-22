[CmdletBinding()]
param(
    [string] $SdkRoot,
    [string] $SysConfigRoot,
    [string] $Compiler,
    [string] $CcsRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectDir = Join-Path $ProjectRoot 'mspm0g3507_bringup'
$BuildDir = Join-Path $ProjectDir 'Debug'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$toolchain = Get-ToolchainConfig -SdkRoot $SdkRoot -SysConfigRoot $SysConfigRoot -Compiler $Compiler -CcsRoot $CcsRoot
Assert-ToolchainConfig -Config $toolchain -Required @('SdkRoot', 'SysConfig', 'Compiler')
$SdkRoot = $toolchain.SdkRoot
$SysConfig = $toolchain.SysConfig
$Compiler = $toolchain.Compiler

function Invoke-CheckedCommand {
    param(
        [Parameter(Mandatory)] [string] $FilePath,
        [Parameter(Mandatory)] [string[]] $Arguments,
        [Parameter(Mandatory)] [string] $Description
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed (exit code $LASTEXITCODE)."
    }
}

if (-not (Test-Path -LiteralPath $SysConfig) -or -not (Test-Path -LiteralPath $Compiler)) {
    throw 'CCS 20.2, SysConfig 1.26.2, or TI Clang 4.0.3.LTS is unavailable at the configured paths.'
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

Invoke-CheckedCommand -FilePath $SysConfig -Arguments @(
    '--script', (Join-Path $ProjectDir 'mspm0g3507_bringup.syscfg'),
    '-o', $BuildDir,
    '-s', (Join-Path $SdkRoot '.metadata\product.json'),
    '--compiler', 'ticlang'
) -Description 'Generating MSPM0G3507 SysConfig files'

$commonCompilerArguments = @(
    '-c', '@device.opt', '-march=thumbv6m', '-mcpu=cortex-m0plus', '-mfloat-abi=soft',
    '-mlittle-endian', '-mthumb', '-O2', "-I$ProjectDir", "-I$BuildDir",
    "-I$SdkRoot\source\third_party\CMSIS\Core\Include", "-I$SdkRoot\source", '-gdwarf-3'
)

Push-Location $BuildDir
try {
    $sources = @(
        @{ Source = (Join-Path $ProjectDir 'board_led.c'); Object = 'board_led.o' },
        @{ Source = (Join-Path $ProjectDir 'board_uart.c'); Object = 'board_uart.o' },
        @{ Source = (Join-Path $ProjectDir 'echo_queue.c'); Object = 'echo_queue.o' },
        @{ Source = (Join-Path $ProjectDir 'main.c'); Object = 'main.o' },
        @{ Source = (Join-Path $BuildDir 'ti_msp_dl_config.c'); Object = 'ti_msp_dl_config.o' },
        @{ Source = (Join-Path $SdkRoot 'source\ti\devices\msp\m0p\startup_system_files\ticlang\startup_mspm0g350x_ticlang.c'); Object = 'startup_mspm0g350x_ticlang.o' }
    )

    foreach ($source in $sources) {
        Invoke-CheckedCommand -FilePath $Compiler -Arguments ($commonCompilerArguments + @('-o', $source.Object, $source.Source)) -Description "Compiling $($source.Object)"
    }

    Invoke-CheckedCommand -FilePath $Compiler -Arguments @(
        '@device.opt', '-march=thumbv6m', '-mcpu=cortex-m0plus', '-mfloat-abi=soft', '-mlittle-endian', '-mthumb', '-O2', '-gdwarf-3',
        '-Wl,-mmspm0g3507_bringup.map', "-Wl,-i$SdkRoot\source", "-Wl,-i$ProjectDir", "-Wl,-i$BuildDir", '-Wl,--diag_wrap=off',
        '-Wl,--display_error_number', '-Wl,--warn_sections', '-Wl,--rom_model', '-o', 'mspm0g3507_bringup.out',
        'board_led.o', 'board_uart.o', 'echo_queue.o', 'main.o', 'ti_msp_dl_config.o', 'startup_mspm0g350x_ticlang.o',
        '-Wl,-ldevice_linker.cmd', '-Wl,-ldevice.cmd.genlibs', '-Wl,-llibc.a'
    ) -Description 'Linking MSPM0G3507 firmware'
}
finally {
    Pop-Location
}
