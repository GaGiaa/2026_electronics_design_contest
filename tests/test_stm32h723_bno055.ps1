[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$output = Join-Path $env:TEMP 'stm32h723_bno055_test.exe'
$servicePath = Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_bno055_service.c'
$usartPath = Join-Path $ProjectRoot 'stm32h723_app\Core\Src\usart.c'
$configPath = Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h'
foreach ($required in @('app_bno055_set_drain', '__HAL_UART_CLEAR_OREFLAG', 'HAL_UARTEx_EnableFifoMode')) {
    $source = if ($required -match 'huart1|FifoMode') { Get-Content -Raw -LiteralPath $usartPath } else { Get-Content -Raw -LiteralPath $servicePath }
    if ($source -notmatch $required) { throw "Missing BNO055 UART recovery requirement: $required" }
}
$config = Get-Content -Raw -LiteralPath $configPath
if ($config -notmatch '#define\s+APP_BNO055_UART_TIMEOUT_MS\s+20U') {
    throw 'BNO055 UART transaction timeout must be 20 ms.'
}
try {
    & $gcc '-std=c99' '-Wall' '-Wextra' '-Werror' "-I$ProjectRoot\stm32h723_app\App\Inc" `
        "$ProjectRoot\stm32h723_app\App\Src\app_bno055.c" `
        "$PSScriptRoot\test_stm32h723_bno055.c" '-lm' '-o' $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 BNO055 test build failed.' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'STM32H723 BNO055 test execution failed.' }
    Write-Output 'STM32H723 BNO055 unit tests passed.'
}
finally { Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue }
