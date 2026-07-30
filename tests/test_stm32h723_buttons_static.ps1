[CmdletBinding()]
param(
    [string] $ProjectRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

function Require-Text {
    param(
        [string] $Text,
        [string] $Pattern,
        [string] $Message
    )

    if ($Text -notmatch [regex]::Escape($Pattern)) {
        throw $Message
    }
}

$appRoot = Join-Path $ProjectRoot 'stm32h723_app'
$ioc = Get-Content -LiteralPath (Join-Path $appRoot 'stm32h723_app.ioc') -Raw
$main = Get-Content -LiteralPath (Join-Path $appRoot 'Core\Inc\main.h') -Raw
$gpio = Get-Content -LiteralPath (Join-Path $appRoot 'Core\Src\gpio.c') -Raw
$config = Get-Content -LiteralPath (Join-Path $appRoot 'App\Inc\app_config.h') -Raw
$buttonsHeader = Get-Content -LiteralPath (Join-Path $appRoot 'App\Inc\app_buttons.h') -Raw
$buttonsSource = Get-Content -LiteralPath (Join-Path $appRoot 'App\Src\app_buttons.c') -Raw
$freertos = Get-Content -LiteralPath (Join-Path $appRoot 'Core\Src\freertos.c') -Raw
$telemetry = Get-Content -LiteralPath (Join-Path $appRoot 'App\Src\app_telemetry.c') -Raw
$debug = Get-Content -LiteralPath (Join-Path $appRoot 'App\Inc\app_debug.h') -Raw
$project = Get-Content -LiteralPath (Join-Path $appRoot 'MDK-ARM\stm32h723_app.uvprojx') -Raw

foreach ($line in @(
    'Mcu.PinsNb=33',
    'PC4.GPIO_Label=BUTTON_PC4',
    'PC4.Signal=GPIO_Input',
    'PC5.GPIO_Label=BUTTON_PC5',
    'PC5.Signal=GPIO_Input',
    'PA6.GPIO_Label=BUTTON_PA6',
    'PA6.Signal=GPIO_Input',
    'PA4.GPIO_Label=BUTTON_LOGIC_HIGH',
    'PA4.Signal=GPIO_Output'
)) {
    Require-Text $ioc $line "Missing button CubeMX configuration: $line"
}

foreach ($pin in @('PC4', 'PC5', 'PA6', 'PA4')) {
    if ($ioc -notmatch "(?m)^Mcu\.Pin\d+=$([regex]::Escape($pin))\r?$") {
        throw "Missing button CubeMX pin assignment: $pin"
    }
}

foreach ($reservedPin in @('PA5.', 'PA7.')) {
    if ($ioc -match [regex]::Escape($reservedPin)) {
        throw "Reserved button pin must remain untouched: $reservedPin"
    }
}

foreach ($line in @(
    '#define BUTTON_PC4_Pin GPIO_PIN_4',
    '#define BUTTON_PC4_GPIO_Port GPIOC',
    '#define BUTTON_PC5_Pin GPIO_PIN_5',
    '#define BUTTON_PC5_GPIO_Port GPIOC',
    '#define BUTTON_PA6_Pin GPIO_PIN_6',
    '#define BUTTON_PA6_GPIO_Port GPIOA',
    '#define BUTTON_LOGIC_HIGH_Pin GPIO_PIN_4',
    '#define BUTTON_LOGIC_HIGH_GPIO_Port GPIOA'
)) {
    Require-Text $main $line "Missing button GPIO label: $line"
}

foreach ($line in @(
    'GPIO_InitStruct.Pin = BUTTON_PC4_Pin|BUTTON_PC5_Pin;',
    'GPIO_InitStruct.Pin = BUTTON_PA6_Pin;',
    'GPIO_InitStruct.Pin = BUTTON_LOGIC_HIGH_Pin;',
    'GPIO_InitStruct.Mode = GPIO_MODE_INPUT;',
    'GPIO_InitStruct.Pull = GPIO_NOPULL;',
    'HAL_GPIO_WritePin(GPIOA, BUTTON_LOGIC_HIGH_Pin, GPIO_PIN_SET);'
)) {
    Require-Text $gpio $line "Missing button GPIO initialization: $line"
}

$pcAnalogBlock = [regex]::Match($gpio, '(?s)/\*Configure GPIO pins : PC.*?HAL_GPIO_Init\(GPIOC, &GPIO_InitStruct\);').Value
if ($pcAnalogBlock -match 'GPIO_PIN_4|GPIO_PIN_5') {
    throw 'PC4/PC5 must not remain in the analog GPIO group.'
}
$paAnalogBlock = [regex]::Match($gpio, '(?s)/\*Configure GPIO pins : PA.*?HAL_GPIO_Init\(GPIOA, &GPIO_InitStruct\);').Value
if ($paAnalogBlock -match 'GPIO_PIN_6') {
    throw 'PA6 must not remain in the analog GPIO group.'
}
$paAnalogBlock = [regex]::Match($gpio, '(?s)/\*Configure GPIO pins : PA.*?HAL_GPIO_Init\(GPIOA, &GPIO_InitStruct\);').Value
if ($paAnalogBlock -match 'GPIO_PIN_4') {
    throw 'PA4 must not remain in the analog GPIO group.'
}

foreach ($line in @(
    '#define APP_H723_BUTTON_VOFA_TELEMETRY_ENABLE 0U',
    '#define APP_H723_BUTTON_VOFA_TELEMETRY_INTERVAL_MS 20U',
    '#define APP_H723_BUTTON_TASK_PERIOD_MS 5U',
    '#define APP_H723_BUTTON_DEBOUNCE_SAMPLES 2U',
    'APP_H723_BUTTON_VOFA_TELEMETRY_ENABLE == 1U'
)) {
    Require-Text $config $line "Missing button configuration: $line"
}

foreach ($line in @(
    'H723_APP_BUTTON_PC5 = 0U',
    'H723_APP_BUTTON_PC4,',
    'H723_APP_BUTTON_PA6',
    'h723_app_buttons_init',
    'h723_app_buttons_step',
    'h723_app_buttons_snapshot_copy'
)) {
    Require-Text $buttonsHeader $line "Missing button interface: $line"
}

foreach ($line in @(
    '{GPIOC, GPIO_PIN_5}',
    '{GPIOC, GPIO_PIN_4}',
    '{GPIOA, GPIO_PIN_6}',
    'APP_H723_BUTTON_DEBOUNCE_SAMPLES',
    's_stable_high_mask'
)) {
    Require-Text $buttonsSource $line "Missing button implementation: $line"
}

foreach ($line in @(
    'buttonTask',
    'startButtonTask',
    'APP_H723_BUTTON_TASK_PERIOD_MS',
    'h723_app_buttons_step'
)) {
    Require-Text $freertos $line "Missing button task integration: $line"
}

foreach ($line in @(
    'H723_VOFA_BUTTON_CHANNEL_COUNT 3U',
    'APP_H723_BUTTON_VOFA_TELEMETRY_ENABLE',
    'H723_APP_BUTTON_PC5',
    'H723_APP_BUTTON_PC4',
    'H723_APP_BUTTON_PA6',
    'HAL_UART_Transmit_DMA(&huart8, s_button_frame'
)) {
    Require-Text $telemetry $line "Missing button VOFA integration: $line"
}

foreach ($line in @(
    'h723_debug_buttons_t',
    'raw_high_mask',
    'stable_high_mask',
    'sample_sequence'
)) {
    Require-Text $debug $line "Missing button debug snapshot: $line"
}

Require-Text $project '<FilePath>../App/Src/app_buttons.c</FilePath>' 'Keil project does not include app_buttons.c.'
Require-Text $telemetry 'APP_H723_BUTTON_VOFA_TELEMETRY_ENABLE << 4U' 'Button telemetry debug bit is missing.'

Write-Output 'STM32H723 button static integration test passed.'
