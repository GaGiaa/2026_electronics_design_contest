[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$config = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h')
$service = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_jy901s_service.c')
$telemetry = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_telemetry.c')
$calibration = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_jy901s_calibration.c')

foreach ($text in @($config, $service, $telemetry, $calibration)) {
    if ($text -match 'TODO|TBD') { throw 'JY901S calibration source contains an unfinished marker.' }
}
foreach ($macro in @(
    'APP_JY901S_CALIBRATION_ENABLE',
    'APP_JY901S_CALIBRATION_SAMPLE_TARGET',
    'APP_JY901S_CALIBRATION_TIMEOUT_MS',
    'APP_JY901S_VOFA_CALIBRATED_ENABLE',
    'APP_JY901S_VEHICLE_X_SENSOR_AXIS',
    'APP_JY901S_VEHICLE_X_SENSOR_SIGN'
)) {
    if ($config -notmatch [regex]::Escape($macro)) { throw "Missing JY901S calibration macro: $macro" }
}
foreach ($textPattern in @(
    'app_jy901s_calibration_init',
    'app_jy901s_calibration_update',
    'vehicle_acceleration_g',
    'vehicle_angular_rate_dps',
    'APP_JY901S_VOFA_CALIBRATED_ENABLE'
)) {
    if (($service + $telemetry + $calibration) -notmatch [regex]::Escape($textPattern)) {
        throw "Missing JY901S calibration integration: $textPattern"
    }
}
if ($service -match 'new_sample') { throw 'JY901S service must not discard intermediate complete samples.' }
if ($service -notmatch 'if \(app_jy901s_parser_feed\([\s\S]{0,320}app_jy901s_calibration_update') {
    throw 'JY901S calibration must update for each newly completed parser sample.'
}

Write-Output 'STM32H723 JY901S calibration static integration test passed.'
