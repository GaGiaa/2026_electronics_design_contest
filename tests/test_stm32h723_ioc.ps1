[CmdletBinding()]
param(
    [string] $ProjectRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

$iocPath = Join-Path $ProjectRoot 'stm32h723_app\stm32h723_app.ioc'
$appConfigPath = Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h'
if (-not (Test-Path -LiteralPath $iocPath)) {
    throw "Missing CubeMX configuration: $iocPath"
}
if (-not (Test-Path -LiteralPath $appConfigPath)) {
    throw "Missing H723 application configuration: $appConfigPath"
}

$ioc = Get-Content -LiteralPath $iocPath -Raw
$appConfig = Get-Content -LiteralPath $appConfigPath -Raw
$requiredLines = @(
    'Mcu.CPN=STM32H723ZGT6',
    'Mcu.Name=STM32H723ZGTx',
    'UART8.BaudRate=1000000',
    'UART8.Mode=MODE_TX',
    'USART1.BaudRate=115200',
    'Dma.UART7_RX.0.Instance=DMA1_Stream0',
    'Dma.UART8_TX.1.Instance=DMA1_Stream1',
    'UART7.BaudRate=420000',
    'UART9.BaudRate=234000',
    'PE7.Signal=UART7_RX',
    'PE8.Signal=UART7_TX',
    'PB6.Signal=USART1_TX',
    'PB7.Signal=USART1_RX',
    'PG0.Signal=UART9_RX',
    'PG1.Signal=UART9_TX',
    'NVIC.DMA1_Stream0_IRQn=true',
    'NVIC.DMA1_Stream2_IRQn=true',
    'NVIC.UART7_IRQn=true',
    'NVIC.UART9_IRQn=true',
    'Dma.UART9_RX.2.Instance=DMA1_Stream2',
    'FDCAN1.CalculateBaudRateNominal=1000000',
    'FDCAN2.CalculateBaudRateNominal=1000000',
    'FDCAN3.CalculateBaudRateNominal=1000000',
    'FDCAN1.MessageRAMOffset=0',
    'PD0.Locked=true',
    'PD0.Signal=FDCAN1_RX',
    'PD1.Locked=true',
    'PD1.Signal=FDCAN1_TX',
    'FDCAN2.MessageRAMOffset=89',
    'FDCAN3.MessageRAMOffset=178',
    'VP_FREERTOS_VS_CMSIS_V2.Mode=CMSIS_V2',
    'CORTEX_M7.CPU_DCache=Disabled',
    'PA13(JTMS/SWDIO).Mode=Serial_Wire',
    'PA14(JTCK/SWCLK).Mode=Serial_Wire',
    'ADC1.ClockPrescaler=ADC_CLOCK_ASYNC_DIV2',
    'ADC1.SamplingTime-0\#ChannelRegularConversion=ADC_SAMPLETIME_64CYCLES_5',
    'RCC.ADCClockSelection=RCC_ADCCLKSOURCE_CLKP',
    'RCC.ADCFreq_Value=64000000',
    'PG3.Signal=GPIO_Output',
    'PG4.Signal=GPIO_Output',
    'PG5.Signal=GPIO_Output'
)

foreach ($line in $requiredLines) {
    if ($ioc -notmatch [regex]::Escape($line)) {
        throw "Missing expected CubeMX configuration: $line"
    }
}

foreach ($removedLine in @('X-CUBE-ALGOBUILD', 'X-CUBE-TOF')) {
    if ($ioc -match [regex]::Escape($removedLine)) {
        throw "Unexpected retained configuration: $removedLine"
    }
}

$fdcanSource = Get-Content -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\Core\Src\fdcan.c') -Raw
foreach ($line in @('PD0     ------> FDCAN1_RX', 'PD1     ------> FDCAN1_TX', 'HAL_GPIO_Init(GPIOD, &GPIO_InitStruct)')) {
    if ($fdcanSource -notmatch [regex]::Escape($line)) {
        throw "CubeMX did not generate expected FDCAN1 GPIO initialization: $line"
    }
}

if ($appConfig -notmatch '#define\s+APP_VOFA_HEALTH_TELEMETRY_ENABLE\s+0U') {
    throw 'APP_VOFA_HEALTH_TELEMETRY_ENABLE must default to 0U.'
}
if ($appConfig -notmatch '#define\s+APP_JY901S_VOFA_TELEMETRY_ENABLE\s+1U') {
    throw 'APP_JY901S_VOFA_TELEMETRY_ENABLE must be enabled for the current JY901S calibration telemetry configuration.'
}

foreach ($removedLine in @('USART6.', 'PC6.Signal=USART6_TX', 'PC7.Signal=USART6_RX')) {
    if ($ioc -match [regex]::Escape($removedLine)) {
        throw "Unexpected retained BNO055 USART6 configuration: $removedLine"
    }
}
if ($appConfig -notmatch '#define\s+APP_BNO055_VOFA_TELEMETRY_ENABLE\s+0U') {
    throw 'APP_BNO055_VOFA_TELEMETRY_ENABLE must default to 0U.'
}
if ($appConfig -notmatch 'APP_BNO055_VOFA_TELEMETRY_ENABLE') {
    throw 'BNO055 telemetry must participate in the UART8 compile-time exclusion.'
}
if ($appConfig -notmatch '#define\s+APP_H723_CHASSIS_ACTUATION_ENABLE\s+(?:0U|1U)') {
    throw 'APP_H723_CHASSIS_ACTUATION_ENABLE must be configured as 0U or 1U.'
}
if ($appConfig -notmatch '#define\s+APP_H723_M2006_FDCAN_INSTANCE\s+2U') {
    throw 'M2006 must default to FDCAN2.'
}
if ($ioc -notmatch '(?m)^Mcu\.IP\d+=ADC1\r?$') {
    throw 'ADC1 must be enabled as a CubeMX peripheral.'
}
if ($ioc -notmatch '(?m)^PA2\.Signal=ADC(?:1|x)_INP14\r?$') {
    throw 'PA2 must be configured as ADC input channel 14.'
}
foreach ($line in @(
    '#define APP_GRAYSCALE_TASK_PERIOD_MS 10U',
    '#define APP_GRAYSCALE_VOFA_TELEMETRY_ENABLE 0U',
    '#define APP_GRAYSCALE_VOFA_TELEMETRY_INTERVAL_MS 100U'
)) {
    if ($appConfig -notmatch [regex]::Escape($line)) {
        throw "Missing grayscale configuration: $line"
    }
}

$adcSource = Get-Content -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\Core\Src\adc.c') -Raw
if ($adcSource -notmatch 'HAL_ADCEx_Calibration_Start\(\&hadc1,\s*ADC_CALIB_OFFSET_LINEARITY,\s*ADC_SINGLE_ENDED\)') {
    throw 'ADC1 must perform offset and linearity calibration during initialization.'
}
if ($adcSource -match 'ADC_DATAALIGN_RIGHT') {
    throw 'H723 ADC1/ADC2 HAL does not define ADC_DATAALIGN_RIGHT; generated adc.c must use fixed right alignment.'
}
if ($appConfig -notmatch '#define\s+APP_H723_SINGLE_MOTOR_PID_DEBUG_ENABLE\s+[01]U') {
    throw 'Single-motor PID debug must be configured as 0U or 1U.'
}
if ($appConfig -notmatch '#define\s+APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_ENABLE\s+[01]U') {
    throw 'Single-motor VOFA telemetry must be configured as 0U or 1U.'
}
if ($appConfig -notmatch '#define\s+APP_H723_SINGLE_MOTOR_VOFA_TELEMETRY_INTERVAL_MS\s+1U') {
    throw 'Single-motor VOFA telemetry interval must default to 1 ms.'
}
foreach ($line in @(
    '#define APP_H723_SINGLE_MOTOR_POSITION_PID_PERIOD_MS 5U',
    '#define APP_H723_SINGLE_MOTOR_POSITION_PID_KP 2.0f',
    '#define APP_H723_SINGLE_MOTOR_POSITION_PID_KI 0.0f',
    '#define APP_H723_SINGLE_MOTOR_POSITION_PID_KD 0.0f',
    '#define APP_H723_SINGLE_MOTOR_POSITION_PID_OUTPUT_LIMIT_RPM APP_H723_SINGLE_MOTOR_MAX_OUTPUT_RPM',
    '#define APP_H723_SINGLE_MOTOR_POSITION_PID_DEADBAND_DEG 0.0f',
    '#define APP_H723_M2006_POSITION_TRACKER_MAX_GAP_MS APP_H723_M2006_FEEDBACK_TIMEOUT_MS'
)) {
    if ($appConfig -notmatch [regex]::Escape($line)) {
        throw "Missing single-motor position configuration: $line"
    }
}
foreach ($line in @(
    '#define APP_H723_CHASSIS_MAX_OUTPUT_RPM 550.0f',
    '#define APP_H723_M2006_PID_KP 0.25f',
    '#define APP_H723_M2006_PID_KI 5.0f',
    '#define APP_H723_M2006_PID_KD 0.0f',
    '#define APP_H723_M2006_PID_OUTPUT_DELTA_LIMIT 0.0f',
    '#define APP_H723_M2006_PID_DEADBAND_RPM 0.1f'
)) {
    if ($appConfig -notmatch [regex]::Escape($line)) {
        throw "Missing chassis PID tuning default: $line"
    }
}
if ($appConfig -notmatch '#if\s+\(APP_H723_M2006_FDCAN_INSTANCE\s+<\s+1U\)\s+\|\|\s+\(APP_H723_M2006_FDCAN_INSTANCE\s+>\s+3U\)') {
    throw 'M2006 FDCAN selection must reject instances outside 1U..3U.'
}

$chassisHeader = Get-Content -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_chassis_service.h') -Raw
if ($chassisHeader -notmatch 'void\s+h723_chassis_on_fdcan3_rx\(void\);') {
    throw 'Chassis service must expose the FDCAN3 receive callback.'
}

$chassisSource = Get-Content -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_chassis_service.c') -Raw
foreach ($line in @('return &hfdcan3;', 'void h723_chassis_on_fdcan3_rx(void)')) {
    if ($chassisSource -notmatch [regex]::Escape($line)) {
        throw "Chassis service must support FDCAN3: $line"
    }
}
if ($chassisSource -notmatch 'header.Identifier\s*<=\s*0x203U') {
    throw 'Single-motor debug must accept M2006 feedback ID 0x203.'
}
foreach ($token in @(
    'app_m2006_position_tracker_update',
    'APP_H723_SINGLE_MOTOR_POSITION_PID_PERIOD_MS',
    'APP_H723_M2006_POSITION_TRACKER_MAX_GAP_MS'
)) {
    if ($chassisSource -notmatch [regex]::Escape($token)) {
        throw "Single-motor position mode is missing: $token"
    }
}
$singleMotorSource = Get-Content -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_single_motor.c') -Raw
if ($singleMotorSource -notmatch [regex]::Escape('PID_Position_Calc')) {
    throw 'Single-motor position mode must calculate the PID in the pure C controller.'
}

$telemetrySource = Get-Content -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_telemetry.c') -Raw
foreach ($channel in @(
    'g_h723_debug.single_motor.target_position_deg',
    'g_h723_debug.single_motor.feedback_position_deg',
    'g_h723_debug.single_motor.position_p_out_rpm',
    'g_h723_debug.single_motor.position_i_out_rpm',
    'g_h723_debug.single_motor.position_d_out_rpm',
    'g_h723_debug.single_motor.position_target_output_speed_rpm',
    'g_h723_debug.single_motor.feedback_output_speed_rpm',
    'g_h723_debug.single_motor.target_current_A',
    'g_h723_debug.single_motor.feedback_current_A'
)) {
    if ($telemetrySource -notmatch [regex]::Escape($channel)) {
        throw "Single-motor position VOFA channel is missing: $channel"
    }
}

if ($fdcanSource -notmatch 'hfdcan->Instance == FDCAN3' -or
    $fdcanSource -notmatch 'h723_chassis_on_fdcan3_rx\(\);') {
    throw 'CubeMX FDCAN callback user code must route FDCAN3 feedback.'
}

Write-Output 'STM32H723 CubeMX configuration test passed.'
