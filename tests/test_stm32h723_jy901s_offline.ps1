[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

function Require-Text([string] $Content, [string] $Pattern, [string] $Message) {
    if ($Content -notmatch $Pattern) { throw $Message }
}

$appRoot = Join-Path $ProjectRoot 'stm32h723_app'
$config = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Inc\app_config.h')
$main = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'Core\Src\main.c')
$freertos = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'Core\Src\freertos.c')
$usart = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'Core\Src\usart.c')
$chassis = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'App\Src\app_chassis_service.c')
$project = Get-Content -Raw -LiteralPath (Join-Path $appRoot 'MDK-ARM\stm32h723_app.uvprojx')

Require-Text $config '#define\s+APP_H723_JY901S_SERVICE_ENABLE\s+0U' `
    'JY901S service must default to soft-offline.'
Require-Text $config 'APP_H723_JY901S_SERVICE_ENABLE == 0U[\s\S]*APP_JY901S_VOFA_TELEMETRY_ENABLE' `
    'JY901S VOFA must require the service switch.'
Require-Text $main '#if \(APP_H723_JY901S_SERVICE_ENABLE == 1U\)[\s\S]*MX_UART9_Init\(\)' `
    'UART9 initialization must be guarded by the JY901S service switch.'
Require-Text $freertos '#if \(APP_H723_JY901S_SERVICE_ENABLE == 1U\)[\s\S]*startJy901sTask' `
    'JY901S task must be guarded by the service switch.'
Require-Text $freertos '#endif\s*\r?\n\s*static osThreadId_t grayscaleTaskHandle' `
    'Only the JY901S task may be guarded; grayscale must remain available while JY901S is offline.'
Require-Text $freertos '#if \(APP_H723_JY901S_SERVICE_ENABLE == 1U\)\s*\r?\nvoid startJy901sTask[\s\S]*?\r?\n}\s*\r?\n#endif\s*\r?\n\s*void startGrayscaleTask' `
    'The JY901S function guard must end before the grayscale task function.'
Require-Text $usart '#if \(APP_H723_JY901S_SERVICE_ENABLE == 1U\)[\s\S]*h723_jy901s_on_uart9_rx_event' `
    'UART9 receive dispatch must be guarded by the service switch.'
Require-Text $usart '#if \(APP_H723_JY901S_SERVICE_ENABLE == 1U\)[\s\S]*h723_jy901s_on_uart9_error' `
    'UART9 error dispatch must be guarded by the service switch.'

if ($chassis -match 'app_jy901s_service\.h|h723_jy901s_service_get_snapshot') {
    throw 'The ID3 direct-position path must not depend on JY901S.'
}
foreach ($line in @(
    '<FilePath>../App/Src/app_jy901s.c</FilePath>',
    '<FilePath>../App/Src/app_jy901s_calibration.c</FilePath>',
    '<FilePath>../App/Src/app_jy901s_service.c</FilePath>'
)) { Require-Text $project ([regex]::Escape($line)) "JY901S recovery source is missing: $line" }

Write-Output 'STM32H723 JY901S soft-offline static test passed.'
