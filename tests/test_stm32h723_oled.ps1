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
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing required OLED file: $RelativePath"
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
$debugHeader = Read-RequiredFile 'stm32h723_app\App\Inc\app_debug.h'
$debugSource = Read-RequiredFile 'stm32h723_app\App\Src\app_debug.c'
$oledHeader = Read-RequiredFile 'stm32h723_app\App\Inc\app_oled.h'
$oledSource = Read-RequiredFile 'stm32h723_app\App\Src\app_oled.c'
$fontHeader = Read-RequiredFile 'stm32h723_app\App\Inc\oled_font.h'
$fontSource = Read-RequiredFile 'stm32h723_app\App\Src\oled_font.c'
$i2cHeader = Read-RequiredFile 'stm32h723_app\Core\Inc\i2c.h'
$i2cSource = Read-RequiredFile 'stm32h723_app\Core\Src\i2c.c'
$project = Read-RequiredFile 'stm32h723_app\MDK-ARM\stm32h723_app.uvprojx'

foreach ($line in @(
    'I2C4',
    'I2C4.Timing=0x10A20D1F',
    'I2C4.FASTMODEPLUS=I2C_FASTMODEPLUS_I2C4',
    'I2C4.I2C_Speed_Mode=I2C_Fast_Plus',
    'I2C4.Speed=1000',
    'I2C4.I2C_Rise_Time=100',
    'I2C4.I2C_Fall_Time=100',
    'I2C4.AddressingMode=I2C_ADDRESSINGMODE_7BIT',
    'I2C4.AnalogFilter=I2C_ANALOGFILTER_ENABLE',
    'PD12.Signal=I2C4_SCL',
    'PD13.Signal=I2C4_SDA'
)) {
    Require-Text $ioc ([regex]::Escape($line)) "Missing I2C4 CubeMX setting: $line"
}

foreach ($line in @('PB8.Signal=I2C1_SCL', 'PB9.Signal=I2C1_SDA')) {
    if ($ioc -match [regex]::Escape($line)) {
        throw "OLED must not use PB8/PB9: $line"
    }
}

foreach ($line in @(
    'extern I2C_HandleTypeDef hi2c4',
    'void MX_I2C4_Init'
)) {
    Require-Text $i2cHeader ([regex]::Escape($line)) "Missing I2C4 header declaration: $line"
}

foreach ($line in @(
    'hi2c4.Instance = I2C4',
    'hi2c4.Init.Timing = 0x10A20D1F',
    'HAL_I2C_Init\(&hi2c4\)',
    'HAL_I2CEx_EnableFastModePlus\(I2C_FASTMODEPLUS_I2C4\)',
    'PD12.*I2C4_SCL',
    'PD13.*I2C4_SDA',
    'GPIO_AF4_I2C4'
)) {
    Require-Text $i2cSource $line "Missing I2C4 generated initialization: $line"
}

foreach ($line in @(
    '#define APP_H723_OLED_I2C_ADDRESS 0x3CU',
    '#define APP_H723_OLED_TASK_PERIOD_MS 100U',
    '#define APP_H723_OLED_REFRESH_PERIOD_MS 100U',
    '#define APP_H723_OLED_I2C_TIMEOUT_MS'
)) {
    Require-Text $appConfig ([regex]::Escape($line)) "Missing OLED configuration: $line"
}
if ($appConfig -match 'APP_H723_OLED_ENABLE|APP_H723_OLED_DEBUG_MODE_ENABLE') {
    throw 'OLED must not be gated by compile-time enable/debug macros.'
}

foreach ($line in @(
    'MX_I2C4_Init\(\)',
    'oledTask_attributes',
    'osPriorityLow',
    'h723_oled_service_init',
    'h723_oled_service_step',
    'osThreadNew\(startOledTask'
)) {
    $text = if ($line -eq 'MX_I2C4_Init\(\)') { $main } else { $freertos }
    Require-Text $text $line "Missing OLED task integration: $line"
}

foreach ($line in @(
    'h723_debug_oled_t',
    'h723_debug_control_t',
    'oled;',
    'initialized',
    'last_hal_status',
    'i2c_error_code',
    'i2c_state',
    'i2c_isr',
    'gpio_pd_idr',
    'i2c_recovery_count',
    'i2c_recovery_status',
    'update_count',
    'error_count'
)) {
    Require-Text $debugHeader ([regex]::Escape($line)) "Missing OLED debug field: $line"
}

Require-Text $debugSource 'g_h723_debug\s*=\s*\{0U\}' 'Debug state must be initialized in app_debug.c.'

foreach ($line in @(
    '128U',
    '8U',
    '0x00U',
    '0x40U',
    'HAL_I2C_Master_Transmit\s*\(\s*&hi2c4',
    'APP_H723_OLED_I2C_ADDRESS << 1U',
    '0xAEU',
    '0xAFU',
    'board_oled_write_string',
    'render_runtime_page',
    'REMOTE CONTROL',
    'TASK MENU',
    'TASK 2',
    'STATE:%s',
    'DIST:%lum',
    'TIME:%lu.%03lus',
    'g_h723_debug.control',
    'g_h723_debug.task2',
    'g_h723_debug.oled.i2c_error_code',
    'g_h723_debug.oled.i2c_recovery_count',
    'hi2c4.ErrorCode',
    'I2C4->ISR',
    'GPIOD->IDR',
    'HAL_I2C_DeInit\s*\(\s*&hi2c4\s*\)',
    'MX_I2C4_Init\s*\(\s*\)',
    'APP_H723_OLED_REFRESH_PERIOD_MS',
    'g_h723_debug.oled.error_count\+\+',
    's_service_initialized = false'
)) {
    Require-Text $oledSource $line "Missing SSD1306 OLED behavior: $line"
}

foreach ($line in @(
    'BOARD_OLED_FONT_FIRST_ASCII',
    'BOARD_OLED_FONT_LAST_ASCII',
    'BOARD_OLED_FONT_WIDTH'
)) {
    Require-Text $fontHeader ([regex]::Escape($line)) "Missing ASCII font declaration: $line"
}
Require-Text $fontSource 'const uint8_t' 'Missing ASCII font data.'

foreach ($line in @(
    '../Core/Src/i2c.c',
    '../App/Src/app_oled.c',
    '../App/Src/oled_font.c'
)) {
    Require-Text $project ([regex]::Escape($line)) "Missing OLED source in Keil project: $line"
}

Write-Output 'STM32H723 OLED I2C4 static test passed.'
