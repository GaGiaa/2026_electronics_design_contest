[CmdletBinding()]
param(
    [ValidateSet(0, 1)]
    [int] $VofaSpeedPidTelemetryEnable,
    [ValidateSet(0, 1)]
    [int] $ImuTelemetryEnable,
    [ValidateSet(0, 1)]
    [int] $ImuYawEnable,
    [ValidateSet(0, 1)]
    [int] $GrayVofaTelemetryEnable,
    [ValidateSet(1, 2)]
    [int] $EncoderDecodeMode,
    [ValidateSet(0, 1)]
    [int] $RtosMonitorEnable
)

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

$commonCompilerArguments = @('-c', '@device.opt', '-march=thumbv6m', '-mcpu=cortex-m0plus', '-mfloat-abi=soft', '-mlittle-endian', '-mthumb', '-O2', '-gdwarf-3', "-I$ProjectDir", "-I$ProjectDir\motor_pid", "-I$BuildDir", "-I$FreeRtosRoot\include", "-I$FreeRtosPort", "-I$SdkRoot\source\third_party\CMSIS\Core\Include", "-I$SdkRoot\source")
if ($PSBoundParameters.ContainsKey('VofaSpeedPidTelemetryEnable')) {
    $commonCompilerArguments += "-DVOFA_SPEED_PID_TELEMETRY_ENABLE=$VofaSpeedPidTelemetryEnable"
}
if ($PSBoundParameters.ContainsKey('ImuTelemetryEnable')) {
    $commonCompilerArguments += "-DIMU_TELEMETRY_ENABLE=$ImuTelemetryEnable"
}
if ($PSBoundParameters.ContainsKey('ImuYawEnable')) {
    $commonCompilerArguments += "-DIMU_YAW_ENABLE=$ImuYawEnable"
}
if ($PSBoundParameters.ContainsKey('GrayVofaTelemetryEnable')) {
    $commonCompilerArguments += "-DGRAY_VOFA_TELEMETRY_ENABLE=$GrayVofaTelemetryEnable"
}
if ($PSBoundParameters.ContainsKey('EncoderDecodeMode')) {
    $commonCompilerArguments += "-DBOARD_ENCODER_DECODE_MODE=$EncoderDecodeMode"
}
if ($PSBoundParameters.ContainsKey('RtosMonitorEnable')) {
    $commonCompilerArguments += "-DRTOS_MONITOR_ENABLE=$RtosMonitorEnable"
}
$sources = @(
    @{ Source = (Join-Path $ProjectDir 'board_encoder.c'); Object = 'board_encoder.o' },
    @{ Source = (Join-Path $ProjectDir 'encoder_quadrature.c'); Object = 'encoder_quadrature.o' },
    @{ Source = (Join-Path $ProjectDir 'encoder_speed_filter.c'); Object = 'encoder_speed_filter.o' },
    @{ Source = (Join-Path $ProjectDir 'board_buzzer.c'); Object = 'board_buzzer.o' },
    @{ Source = (Join-Path $ProjectDir 'board_bmi160.c'); Object = 'board_bmi160.o' },
    @{ Source = (Join-Path $ProjectDir 'board_imu_yaw.c'); Object = 'board_imu_yaw.o' },
    @{ Source = (Join-Path $ProjectDir 'board_buttons.c'); Object = 'board_buttons.o' },
    @{ Source = (Join-Path $ProjectDir 'board_grayscale.c'); Object = 'board_grayscale.o' },
    @{ Source = (Join-Path $ProjectDir 'line_tracking.c'); Object = 'line_tracking.o' },
    @{ Source = (Join-Path $ProjectDir 'motor_pid\pid.c'); Object = 'motor_pid.o' },
    @{ Source = (Join-Path $ProjectDir 'motor_control.c'); Object = 'motor_control.o' },
    @{ Source = (Join-Path $ProjectDir 'vofa_justfloat.c'); Object = 'vofa_justfloat.o' },
    @{ Source = (Join-Path $ProjectDir 'board_motor.c'); Object = 'board_motor.o' },
    @{ Source = (Join-Path $ProjectDir 'board_ws2812.c'); Object = 'board_ws2812.o' },
    @{ Source = (Join-Path $ProjectDir 'board_uart.c'); Object = 'board_uart.o' },
    @{ Source = (Join-Path $ProjectDir 'rtos_monitor.c'); Object = 'rtos_monitor.o' },
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
    Invoke-CheckedCommand -FilePath $Compiler -Arguments @('@device.opt', '-march=thumbv6m', '-mcpu=cortex-m0plus', '-mfloat-abi=soft', '-mlittle-endian', '-mthumb', '-O2', '-gdwarf-3', '-Wl,-mmspm0g3507_app.map', "-Wl,-i$SdkRoot\source", "-Wl,-i$ProjectDir", "-Wl,-i$ProjectDir\motor_pid", "-Wl,-i$BuildDir", '-Wl,--diag_wrap=off', '-Wl,--display_error_number', '-Wl,--warn_sections', '-Wl,--rom_model', '-o', 'mspm0g3507_app.out', 'board_encoder.o', 'encoder_quadrature.o', 'encoder_speed_filter.o', 'board_buzzer.o', 'board_bmi160.o', 'board_imu_yaw.o', 'board_buttons.o', 'board_grayscale.o', 'line_tracking.o', 'motor_pid.o', 'motor_control.o', 'vofa_justfloat.o', 'board_motor.o', 'board_ws2812.o', 'board_uart.o', 'rtos_monitor.o', 'main.o', 'ti_msp_dl_config.o', 'startup_mspm0g350x_ticlang.o', 'freertos_list.o', 'freertos_queue.o', 'freertos_tasks.o', 'freertos_port.o', 'freertos_portasm.o', '-Wl,-ldevice_linker.cmd', '-Wl,-ldevice.cmd.genlibs', '-Wl,-llibc.a') -Description 'Linking MSPM0G3507 app firmware'
}
finally { Pop-Location }
