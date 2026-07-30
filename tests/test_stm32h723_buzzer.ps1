[CmdletBinding()]
param(
    [string] $ProjectRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

function Read-RequiredFile {
    param([string] $RelativePath)

    $path = Join-Path $ProjectRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing required buzzer file: $RelativePath"
    }
    return Get-Content -LiteralPath $path -Raw
}

function Require-Text {
    param(
        [string] $Text,
        [string] $Pattern,
        [string] $Message
    )

    if ($Text -notmatch $Pattern) {
        throw $Message
    }
}

$ioc = Read-RequiredFile 'stm32h723_app\stm32h723_app.ioc'
$appConfig = Read-RequiredFile 'stm32h723_app\App\Inc\app_config.h'
$main = Read-RequiredFile 'stm32h723_app\Core\Src\main.c'
$freertos = Read-RequiredFile 'stm32h723_app\Core\Src\freertos.c'
$project = Read-RequiredFile 'stm32h723_app\MDK-ARM\stm32h723_app.uvprojx'
$buzzerHeader = Read-RequiredFile 'stm32h723_app\App\Inc\app_buzzer.h'
$buzzerSource = Read-RequiredFile 'stm32h723_app\App\Src\app_buzzer.c'
$timHeader = Read-RequiredFile 'stm32h723_app\Core\Inc\tim.h'
$timSource = Read-RequiredFile 'stm32h723_app\Core\Src\tim.c'

foreach ($line in @(
    '#define APP_H723_BUZZER_TEST_ENABLE 0U',
    '#define APP_H723_BUZZER_FREQUENCY_HZ 2000U',
    '#define APP_H723_BUZZER_DUTY_PERCENT 50U',
    '#define APP_H723_BUZZER_ON_TIME_MS 200U',
    '#define APP_H723_BUZZER_OFF_TIME_MS 1800U',
    'APP_H723_BUZZER_TEST_ENABLE must be 0U or 1U',
    'APP_H723_BUZZER_FREQUENCY_HZ must be between 1000 Hz and 20000 Hz',
    'APP_H723_BUZZER_DUTY_PERCENT must not exceed 100',
    'APP_H723_BUZZER_ON_TIME_MS and APP_H723_BUZZER_OFF_TIME_MS must be nonzero'
)) {
    Require-Text $appConfig ([regex]::Escape($line)) "Missing buzzer configuration: $line"
}

foreach ($line in @(
    'Mcu.IP10=TIM2',
    'PA3.Signal=TIM2_CH4',
    'TIM2.Channel-PWM\ Generation4\ CH4=TIM_CHANNEL_4',
    'TIM2.Prescaler=274',
    'TIM2.Period=499'
)) {
    Require-Text $ioc ([regex]::Escape($line)) "Missing CubeMX buzzer setting: $line"
}
Require-Text $main 'MX_TIM2_Init\(\)' 'Main must initialize TIM2 before the scheduler starts.'

foreach ($line in @(
    'extern TIM_HandleTypeDef htim2',
    'void MX_TIM2_Init',
    'htim2.Instance = TIM2',
    'HAL_TIM_PWM_Start\(&htim2, TIM_CHANNEL_4\)',
    'GPIO_AF1_TIM2'
)) {
    $text = if ($line -match 'extern|MX_TIM2') { $timHeader } elseif ($line -match 'HAL_TIM_PWM_Start') { $buzzerSource } else { $timSource }
    Require-Text $text $line "Missing TIM2 PWM initialization: $line"
}

foreach ($line in @(
    'h723_buzzer_service_init',
    'h723_buzzer_service_step'
)) {
    Require-Text $buzzerHeader ([regex]::Escape($line)) "Missing buzzer service declaration: $line"
    Require-Text $buzzerSource ([regex]::Escape($line)) "Missing buzzer service behavior: $line"
}
Require-Text $buzzerSource 'APP_H723_BUZZER_TEST_ENABLE' 'Buzzer service must honor the compile-time switch.'

foreach ($line in @(
    'h723_buzzer_service_init\(\)',
    'h723_buzzer_service_step\(h723_app_time_now_ms\(\)\)'
)) {
    Require-Text $freertos $line "Missing default task buzzer integration: $line"
}

foreach ($line in @(
    '../App/Src/app_buzzer.c',
    '../Core/Src/tim.c',
    '../Core/Src/stm32h7xx_hal_msp.c'
)) {
    Require-Text $project ([regex]::Escape($line)) "Missing buzzer source in Keil project: $line"
}

Write-Output 'STM32H723 buzzer static test passed.'
