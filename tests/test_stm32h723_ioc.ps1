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
    'Dma.UART8_TX.0.Instance=DMA1_Stream1',
    'FDCAN1.CalculateBaudRateNominal=1000000',
    'FDCAN2.CalculateBaudRateNominal=1000000',
    'FDCAN3.CalculateBaudRateNominal=1000000',
    'FDCAN1.MessageRAMOffset=0',
    'FDCAN2.MessageRAMOffset=89',
    'FDCAN3.MessageRAMOffset=178',
    'VP_FREERTOS_VS_CMSIS_V2.Mode=CMSIS_V2',
    'CORTEX_M7.CPU_DCache=Disabled',
    'PA13(JTMS/SWDIO).Mode=Serial_Wire',
    'PA14(JTCK/SWCLK).Mode=Serial_Wire'
)

foreach ($line in $requiredLines) {
    if ($ioc -notmatch [regex]::Escape($line)) {
        throw "Missing expected CubeMX configuration: $line"
    }
}

foreach ($removedLine in @('UART7.BaudRate=', 'Mcu.IP11=UART7', 'X-CUBE-ALGOBUILD', 'X-CUBE-TOF')) {
    if ($ioc -match [regex]::Escape($removedLine)) {
        throw "Unexpected retained configuration: $removedLine"
    }
}

if ($appConfig -notmatch '#define\s+APP_VOFA_HEALTH_TELEMETRY_ENABLE\s+0U') {
    throw 'APP_VOFA_HEALTH_TELEMETRY_ENABLE must default to 0U.'
}

Write-Output 'STM32H723 CubeMX configuration test passed.'
