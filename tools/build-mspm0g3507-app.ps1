[CmdletBinding()]
param(
    [string] $SdkRoot,
    [string] $SysConfigRoot,
    [string] $Compiler,
    [string] $CcsRoot,
    [ValidateSet(0, 1)]
    [int] $VofaSpeedPidTelemetryEnable,
    [ValidateSet(0, 1)]
    [int] $ButtonVofaTelemetryEnable,
    [ValidateSet(0, 1)]
    [int] $ImuTelemetryEnable,
    [ValidateRange(1, 60000)]
    [int] $ImuVofaTelemetryIntervalMs,
    [ValidateSet(0, 1)]
    [int] $ImuYawEnable,
    [ValidateSet(0, 1)]
    [int] $GrayVofaTelemetryEnable,
    [ValidateSet(0, 1)]
    [int] $LineControlVofaTelemetryEnable,
    [ValidateRange(1, 60000)]
    [int] $LineControlVofaTelemetryIntervalMs,
    [ValidateSet(1, 2)]
    [int] $EncoderDecodeMode,
    [ValidateSet(0, 1)]
    [int] $RtosMonitorEnable,
    [ValidateSet(0, 1)]
    [int] $CrsfRemoteControlEnable,
    [ValidateSet(0, 1)]
    [int] $OledTestTaskEnable,
    [ValidateSet(0, 1)]
    [int] $ServoFeatureEnable
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectDir = Join-Path $ProjectRoot 'mspm0g3507_app'
$BuildDir = Join-Path $ProjectDir 'Debug'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$toolchain = Get-ToolchainConfig -SdkRoot $SdkRoot -SysConfigRoot $SysConfigRoot -Compiler $Compiler -CcsRoot $CcsRoot
Assert-ToolchainConfig -Config $toolchain -Required @('SdkRoot', 'SysConfig', 'Compiler')
$SdkRoot = $toolchain.SdkRoot
$SysConfig = $toolchain.SysConfig
$Compiler = $toolchain.Compiler
$FreeRtosRoot = Join-Path $SdkRoot 'kernel\freertos\Source'
$FreeRtosPort = Join-Path $FreeRtosRoot 'portable\TI_ARM_CLANG\ARM_CM0'

function Invoke-CheckedCommand {
    param([Parameter(Mandatory)] [string] $FilePath, [Parameter(Mandatory)] [string[]] $Arguments, [Parameter(Mandatory)] [string] $Description)
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Description failed (exit code $LASTEXITCODE)." }
}

foreach ($path in @($ProjectDir, $SdkRoot, $SysConfig, $Compiler, $FreeRtosRoot, $FreeRtosPort, (Join-Path $ProjectDir 'config\FreeRTOSConfig.h'))) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Required app build path is unavailable: $path" }
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
Invoke-CheckedCommand -FilePath $SysConfig -Arguments @('--script', (Join-Path $ProjectDir 'mspm0g3507_app.syscfg'), '-o', $BuildDir, '-s', (Join-Path $SdkRoot '.metadata\product.json'), '--compiler', 'ticlang') -Description 'Generating MSPM0G3507 app SysConfig files'

$commonCompilerArguments = @('-c', '@device.opt', '-march=thumbv6m', '-mcpu=cortex-m0plus', '-mfloat-abi=soft', '-mlittle-endian', '-mthumb', '-O2', '-gdwarf-3', "-I$ProjectDir", "-I$ProjectDir\app", "-I$ProjectDir\drivers", "-I$ProjectDir\algorithms", "-I$ProjectDir\protocols", "-I$ProjectDir\services", "-I$ProjectDir\platform", "-I$ProjectDir\config", "-I$BuildDir", "-I$FreeRtosRoot\include", "-I$FreeRtosPort", "-I$SdkRoot\source\third_party\CMSIS\Core\Include", "-I$SdkRoot\source")
if ($PSBoundParameters.ContainsKey('VofaSpeedPidTelemetryEnable')) {
    $commonCompilerArguments += "-DAPP_VOFA_SPEED_PID_TELEMETRY_ENABLE=$VofaSpeedPidTelemetryEnable"
}
if ($PSBoundParameters.ContainsKey('ButtonVofaTelemetryEnable')) {
    $commonCompilerArguments += "-DAPP_BUTTON_VOFA_TELEMETRY_ENABLE=$ButtonVofaTelemetryEnable"
}
if ($PSBoundParameters.ContainsKey('ImuTelemetryEnable')) {
    $commonCompilerArguments += "-DAPP_IMU_TELEMETRY_ENABLE=$ImuTelemetryEnable"
}
if ($PSBoundParameters.ContainsKey('ImuVofaTelemetryIntervalMs')) {
    $commonCompilerArguments += "-DAPP_IMU_VOFA_TELEMETRY_INTERVAL_MS=$ImuVofaTelemetryIntervalMs"
}
if ($PSBoundParameters.ContainsKey('ImuYawEnable')) {
    $commonCompilerArguments += "-DAPP_IMU_YAW_ENABLE=$ImuYawEnable"
}
if ($PSBoundParameters.ContainsKey('GrayVofaTelemetryEnable')) {
    $commonCompilerArguments += "-DAPP_GRAY_VOFA_TELEMETRY_ENABLE=$GrayVofaTelemetryEnable"
}
if ($PSBoundParameters.ContainsKey('LineControlVofaTelemetryEnable')) {
    $commonCompilerArguments += "-DAPP_LINE_CONTROL_VOFA_TELEMETRY_ENABLE=$LineControlVofaTelemetryEnable"
}
if ($PSBoundParameters.ContainsKey('LineControlVofaTelemetryIntervalMs')) {
    $commonCompilerArguments += "-DAPP_LINE_CONTROL_VOFA_TELEMETRY_INTERVAL_MS=$LineControlVofaTelemetryIntervalMs"
}
if ($PSBoundParameters.ContainsKey('EncoderDecodeMode')) {
    $commonCompilerArguments += "-DBOARD_ENCODER_DECODE_MODE=$EncoderDecodeMode"
}
if ($PSBoundParameters.ContainsKey('RtosMonitorEnable')) {
    $commonCompilerArguments += "-DAPP_RTOS_MONITOR_ENABLE=$RtosMonitorEnable"
}
if ($PSBoundParameters.ContainsKey('CrsfRemoteControlEnable')) {
    $commonCompilerArguments += "-DCRSF_REMOTE_CONTROL_ENABLE=$CrsfRemoteControlEnable"
}
if ($PSBoundParameters.ContainsKey('OledTestTaskEnable')) {
    $commonCompilerArguments += "-DAPP_OLED_TEST_TASK_ENABLE=$OledTestTaskEnable"
}
if ($PSBoundParameters.ContainsKey('ServoFeatureEnable')) {
    $commonCompilerArguments += "-DAPP_SERVO_FEATURE_ENABLE=$ServoFeatureEnable"
}
$sources = @(
    @{ Source = (Join-Path $ProjectDir 'drivers\encoder\board_encoder.c'); Object = 'board_encoder.o' },
    @{ Source = (Join-Path $ProjectDir 'algorithms\encoder\encoder_quadrature.c'); Object = 'encoder_quadrature.o' },
    @{ Source = (Join-Path $ProjectDir 'algorithms\encoder\encoder_speed_filter.c'); Object = 'encoder_speed_filter.o' },
    @{ Source = (Join-Path $ProjectDir 'drivers\buzzer\board_buzzer.c'); Object = 'board_buzzer.o' },
    @{ Source = (Join-Path $ProjectDir 'drivers\servo\board_servo.c'); Object = 'board_servo.o' },
    @{ Source = (Join-Path $ProjectDir 'drivers\imu\board_bmi160.c'); Object = 'board_bmi160.o' },
    @{ Source = (Join-Path $ProjectDir 'algorithms\imu_yaw\board_imu_yaw.c'); Object = 'board_imu_yaw.o' },
    @{ Source = (Join-Path $ProjectDir 'drivers\buttons\board_buttons.c'); Object = 'board_buttons.o' },
    @{ Source = (Join-Path $ProjectDir 'drivers\grayscale\board_grayscale.c'); Object = 'board_grayscale.o' },
    @{ Source = (Join-Path $ProjectDir 'algorithms\line_tracking\line_tracking.c'); Object = 'line_tracking.o' },
    @{ Source = (Join-Path $ProjectDir 'algorithms\line_control\line_control.c'); Object = 'line_control.o' },
    @{ Source = (Join-Path $ProjectDir 'algorithms\yaw_control\yaw_control.c'); Object = 'yaw_control.o' },
    @{ Source = (Join-Path $ProjectDir 'algorithms\pid\pid.c'); Object = 'motor_pid.o' },
    @{ Source = (Join-Path $ProjectDir 'algorithms\motor_control\motor_control.c'); Object = 'motor_control.o' },
    @{ Source = (Join-Path $ProjectDir 'protocols\vofa\vofa_justfloat.c'); Object = 'vofa_justfloat.o' },
    @{ Source = (Join-Path $ProjectDir 'drivers\motor\board_motor.c'); Object = 'board_motor.o' },
    @{ Source = (Join-Path $ProjectDir 'drivers\ws2812\board_ws2812.c'); Object = 'board_ws2812.o' },
    @{ Source = (Join-Path $ProjectDir 'drivers\oled\board_oled.c'); Object = 'board_oled.o' },
    @{ Source = (Join-Path $ProjectDir 'drivers\oled\board_oled_font.c'); Object = 'board_oled_font.o' },
    @{ Source = (Join-Path $ProjectDir 'drivers\uart\board_uart.c'); Object = 'board_uart.o' },
    @{ Source = (Join-Path $ProjectDir 'services\rtos_monitor\rtos_monitor.c'); Object = 'rtos_monitor.o' },
    @{ Source = (Join-Path $ProjectDir 'drivers\crsf_uart\board_crsf_uart.c'); Object = 'board_crsf_uart.o' },
    @{ Source = (Join-Path $ProjectDir 'protocols\crsf\crsf_protocol.c'); Object = 'crsf_protocol.o' },
    @{ Source = (Join-Path $ProjectDir 'protocols\crsf\crsf_control.c'); Object = 'crsf_control.o' },
    @{ Source = (Join-Path $ProjectDir 'app\app_profile.c'); Object = 'app_profile.o' },
    @{ Source = (Join-Path $ProjectDir 'app\app_state.c'); Object = 'app_state.o' },
    @{ Source = (Join-Path $ProjectDir 'app\app_startup.c'); Object = 'app_startup.o' },
    @{ Source = (Join-Path $ProjectDir 'app\app_tasks_motor.c'); Object = 'app_tasks_motor.o' },
    @{ Source = (Join-Path $ProjectDir 'app\app_tasks_sensor.c'); Object = 'app_tasks_sensor.o' },
    @{ Source = (Join-Path $ProjectDir 'app\app_tasks_io.c'); Object = 'app_tasks_io.o' },
    @{ Source = (Join-Path $ProjectDir 'app\app_tasks_telemetry.c'); Object = 'app_tasks_telemetry.o' },
    @{ Source = (Join-Path $ProjectDir 'platform\g3507_interrupts.c'); Object = 'g3507_interrupts.o' },
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
    Invoke-CheckedCommand -FilePath $Compiler -Arguments @('@device.opt', '-march=thumbv6m', '-mcpu=cortex-m0plus', '-mfloat-abi=soft', '-mlittle-endian', '-mthumb', '-O2', '-gdwarf-3', '-Wl,-mmspm0g3507_app.map', "-Wl,-i$SdkRoot\source", "-Wl,-i$ProjectDir", "-Wl,-i$ProjectDir\config", "-Wl,-i$BuildDir", '-Wl,--diag_wrap=off', '-Wl,--display_error_number', '-Wl,--warn_sections', '-Wl,--rom_model', '-o', 'mspm0g3507_app.out', 'board_encoder.o', 'encoder_quadrature.o', 'encoder_speed_filter.o', 'board_buzzer.o', 'board_servo.o', 'board_bmi160.o', 'board_imu_yaw.o', 'board_buttons.o', 'board_grayscale.o', 'line_tracking.o', 'line_control.o', 'yaw_control.o', 'motor_pid.o', 'motor_control.o', 'vofa_justfloat.o', 'board_motor.o', 'board_ws2812.o', 'board_oled.o', 'board_oled_font.o', 'board_uart.o', 'rtos_monitor.o', 'board_crsf_uart.o', 'crsf_protocol.o', 'crsf_control.o', 'app_profile.o', 'app_state.o', 'app_startup.o', 'app_tasks_motor.o', 'app_tasks_sensor.o', 'app_tasks_io.o', 'app_tasks_telemetry.o', 'g3507_interrupts.o', 'main.o', 'ti_msp_dl_config.o', 'startup_mspm0g350x_ticlang.o', 'freertos_list.o', 'freertos_queue.o', 'freertos_tasks.o', 'freertos_port.o', 'freertos_portasm.o', '-Wl,-ldevice_linker.cmd', '-Wl,-ldevice.cmd.genlibs', '-Wl,-llibc.a') -Description 'Linking MSPM0G3507 app firmware'
}
finally { Pop-Location }
