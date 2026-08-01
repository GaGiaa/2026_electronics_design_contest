[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

function Require-Text([string] $Content, [string] $Needle, [string] $Message) {
    if ($Content -notmatch [regex]::Escape($Needle)) { throw $Message }
}

$appRoot = Join-Path $ProjectRoot 'stm32h723_app'
$ioc = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'stm32h723_app.ioc')
$config = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Inc\app_config.h')
$freertos = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'Core\Src\freertos.c')
$usart = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'Core\Src\usart.c')
$telemetry = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Src\app_telemetry.c')
$parser = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Src\app_k230.c')
$parserHeader = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Inc\app_k230.h')
$debug = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Inc\app_debug.h')
$project = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'MDK-ARM\stm32h723_app.uvprojx')

foreach ($line in @(
    'USART2.BaudRate=234000',
    'PD5.Signal=USART2_TX',
    'PD6.Signal=USART2_RX',
    'Dma.USART2_RX.3.Instance=DMA1_Stream3',
    'NVIC.DMA1_Stream3_IRQn=true',
    'NVIC.USART2_IRQn=true'
)) { Require-Text $ioc $line "Missing UART2 CubeMX configuration: $line" }

foreach ($line in @(
    '#define APP_H723_K230_UART2_ENABLE 1U',
    '#define APP_H723_K230_UART2_TEST_ENABLE 0U',
    '#define APP_H723_K230_UART2_TEST_VOFA_INTERVAL_MS 20U',
    'APP_H723_K230_UART2_TEST_ENABLE == 1U'
)) { Require-Text $config $line "Missing K230 test configuration: $line" }

foreach ($line in @(
    'h723_k230_service_init',
    'h723_k230_service_step',
    'APP_H723_K230_UART2_ENABLE'
)) { Require-Text $freertos $line "K230 FreeRTOS integration missing: $line" }

foreach ($line in @(
    'h723_k230_on_uart2_rx_event',
    'h723_k230_on_uart2_error',
    'huart->Instance == USART2'
)) { Require-Text $usart $line "K230 UART2 callback integration missing: $line" }

foreach ($line in @(
    'H723_VOFA_K230_CHANNEL_COUNT 5U',
    'g_h723_debug.ball_vision.pixel_x',
    'g_h723_debug.ball_vision.ball_position_mm',
    'g_h723_debug.ball_vision.pipe_tilt_deg',
    '(float)g_h723_debug.ball_vision.valid',
    '(float)g_h723_debug.ball_vision.frame_age_ms',
    'APP_H723_K230_UART2_TEST_VOFA_INTERVAL_MS'
)) { Require-Text $telemetry $line "K230 VOFA telemetry missing: $line" }

foreach ($line in @(
    'h723_debug_ball_vision_t',
    'h723_debug_ball_vision_t ball_vision;',
    'float pixel_x;',
    'float ball_position_mm;',
    'float pipe_tilt_deg;',
    'uint32_t valid;'
)) { Require-Text $debug $line "K230 debug snapshot missing: $line" }

foreach ($line in @(
    'APP_K230_PX_CENTER 993.0f',
    'APP_K230_PX_PER_MM 6.575f',
    'APP_K230_CAMERA_HEIGHT_MM 24.5f',
    'APP_K230_BALL_TRAVEL_MAX_MM 120.0f',
    'float app_k230_pixel_to_ball_mm'
)) { Require-Text ($parser + "`n" + $parserHeader) $line "K230 pixel conversion missing: $line" }

foreach ($line in @(
    '<FilePath>../App/Src/app_k230.c</FilePath>',
    '<FilePath>../App/Src/app_k230_service.c</FilePath>'
)) { Require-Text $project $line "K230 source absent from Keil project: $line" }

Write-Output 'STM32H723 K230 UART2 static integration test passed.'
