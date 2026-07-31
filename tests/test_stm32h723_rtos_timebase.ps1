[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$timeHeader = Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_time.h'
$timeSource = Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_time.c'
if (-not (Test-Path -LiteralPath $timeHeader)) { throw "Missing RTOS time interface: $timeHeader" }
if (-not (Test-Path -LiteralPath $timeSource)) { throw "Missing RTOS time implementation: $timeSource" }

$header = Get-Content -Raw -LiteralPath $timeHeader
$source = Get-Content -Raw -LiteralPath $timeSource
if ($header -notmatch 'uint32_t\s+h723_app_time_now_ms\s*\(\s*void\s*\)') {
    throw 'RTOS time interface must expose h723_app_time_now_ms(void).'
}
if ($source -notmatch 'osKernelGetTickCount\s*\(\s*\)') {
    throw 'RTOS time implementation must read osKernelGetTickCount().'
}

$freertosSource = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\Core\Src\freertos.c')
$configSource = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h')
if ($configSource -notmatch 'APP_H723_BNO055_SERVICE_ENABLE\s+0U') {
    throw 'BNO055 service must default to disabled.'
}
if ($freertosSource -notmatch '#if \(APP_H723_BNO055_SERVICE_ENABLE == 1U\)[\s\S]*startBno055Task') {
    throw 'BNO055 task must be conditionally compiled behind APP_H723_BNO055_SERVICE_ENABLE.'
}

$applicationSources = @(
    Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App') -Recurse -File -Filter '*.c'
    Get-Item -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\Core\Src\freertos.c')
)
foreach ($file in $applicationSources) {
    if ((Get-Content -Raw -LiteralPath $file.FullName) -match 'HAL_GetTick\s*\(') {
        throw "Application source must not call HAL_GetTick(): $($file.FullName)"
    }
}

Write-Output 'STM32H723 application RTOS timebase test passed.'
