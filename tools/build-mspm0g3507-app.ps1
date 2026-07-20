[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectDir = Join-Path $ProjectRoot 'mspm0g3507_app'
$BuildDir = Join-Path $ProjectDir 'Debug'
$SdkRoot = 'D:\Software\ti\ccs2020\mspm0_sdk_2_11_00_07'
$SysConfig = 'D:\Software\ti\ccs2020\sysconfig_1.26.2\sysconfig_cli.bat'
$Compiler = 'D:\Software\ti\ccs2020\ccs\tools\compiler\ti-cgt-armllvm_4.0.3.LTS\bin\tiarmclang.exe'
$FreeRtosRoot = Join-Path $SdkRoot 'kernel\freertos\Source'
$FreeRtosPort = Join-Path $FreeRtosRoot 'portable\TI_ARM_CLANG\ARM_CM0'

function Invoke-CheckedCommand {
    param([Parameter(Mandatory)] [string] $FilePath, [Parameter(Mandatory)] [string[]] $Arguments, [Parameter(Mandatory)] [string] $Description)
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Description failed (exit code $LASTEXITCODE)." }
}

foreach ($path in @($ProjectDir, $SdkRoot, $SysConfig, $Compiler, $FreeRtosRoot, $FreeRtosPort, (Join-Path $ProjectDir 'FreeRTOSConfig.h'))) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Required app build path is unavailable: $path" }
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
Invoke-CheckedCommand -FilePath $SysConfig -Arguments @('--script', (Join-Path $ProjectDir 'mspm0g3507_app.syscfg'), '-o', $BuildDir, '-s', (Join-Path $SdkRoot '.metadata\product.json'), '--compiler', 'ticlang') -Description 'Generating MSPM0G3507 app SysConfig files'

$commonCompilerArguments = @('-c', '@device.opt', '-march=thumbv6m', '-mcpu=cortex-m0plus', '-mfloat-abi=soft', '-mlittle-endian', '-mthumb', '-O2', '-gdwarf-3', "-I$ProjectDir", "-I$BuildDir", "-I$FreeRtosRoot\include", "-I$FreeRtosPort", "-I$SdkRoot\source\third_party\CMSIS\Core\Include", "-I$SdkRoot\source")
$sources = @(
    @{ Source = (Join-Path $ProjectDir 'board_ws2812.c'); Object = 'board_ws2812.o' },
    @{ Source = (Join-Path $ProjectDir 'board_uart.c'); Object = 'board_uart.o' },
    @{ Source = (Join-Path $ProjectDir 'main.c'); Object = 'main.o' },
    @{ Source = (Join-Path $BuildDir 'ti_msp_dl_config.c'); Object = 'ti_msp_dl_config.o' },
    @{ Source = (Join-Path $SdkRoot 'source\ti\devices\msp\m0p\startup_system_files\ticlang\startup_mspm0g350x_ticlang.c'); Object = 'startup_mspm0g350x_ticlang.o' },
    @{ Source = (Join-Path $FreeRtosRoot 'list.c'); Object = 'freertos_list.o' },
    @{ Source = (Join-Path $FreeRtosRoot 'queue.c'); Object = 'freertos_queue.o' },
    @{ Source = (Join-Path $FreeRtosRoot 'tasks.c'); Object = 'freertos_tasks.o' },
    @{ Source = (Join-Path $FreeRtosPort 'port.c'); Object = 'freertos_port.o' },
    @{ Source = (Join-Path $FreeRtosPort 'portasm.c'); Object = 'freertos_portasm.o' }
)

Push-Location $BuildDir
try {
    foreach ($source in $sources) { Invoke-CheckedCommand -FilePath $Compiler -Arguments ($commonCompilerArguments + @('-o', $source.Object, $source.Source)) -Description "Compiling $($source.Object)" }
    Invoke-CheckedCommand -FilePath $Compiler -Arguments @('@device.opt', '-march=thumbv6m', '-mcpu=cortex-m0plus', '-mfloat-abi=soft', '-mlittle-endian', '-mthumb', '-O2', '-gdwarf-3', '-Wl,-mmspm0g3507_app.map', "-Wl,-i$SdkRoot\source", "-Wl,-i$ProjectDir", "-Wl,-i$BuildDir", '-Wl,--diag_wrap=off', '-Wl,--display_error_number', '-Wl,--warn_sections', '-Wl,--rom_model', '-o', 'mspm0g3507_app.out', 'board_ws2812.o', 'board_uart.o', 'main.o', 'ti_msp_dl_config.o', 'startup_mspm0g350x_ticlang.o', 'freertos_list.o', 'freertos_queue.o', 'freertos_tasks.o', 'freertos_port.o', 'freertos_portasm.o', '-Wl,-ldevice_linker.cmd', '-Wl,-ldevice.cmd.genlibs', '-Wl,-llibc.a') -Description 'Linking MSPM0G3507 app firmware'
}
finally { Pop-Location }
