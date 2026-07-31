[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$jyHeader = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_jy901s_service.h')
$jyService = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_jy901s_service.c')
$chassisService = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_chassis_service.c')
$tiltHeader = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_tilt_control.h')
$tiltService = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Src\app_tilt_control.c')
$freertosSource = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\Core\Src\freertos.c')
$config = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_config.h')
$project = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'stm32h723_app\MDK-ARM\stm32h723_app.uvprojx')

foreach ($required in @(
    'typedef struct \{[\s\S]*vehicle_pitch_deg[\s\S]*complete_sample_count[\s\S]*sample_valid[\s\S]*calibration_valid',
    'bool h723_jy901s_service_get_snapshot\(h723_jy901s_control_snapshot_t \*snapshot,',
    's_control_snapshot_sequence',
    'h723_jy901s_service_get_snapshot\(',
    'imu_pitch_deg\s*=\s*imu_snapshot\.vehicle_pitch_deg',
    'imu_valid\s*=\s*snapshot_available\s*&&\s*imu_snapshot\.sample_valid\s*&&\s*imu_snapshot\.calibration_valid',
    'APP_H723_TILT_CONTROL_PERIOD_MS 5U',
    'APP_H723_BNO055_SERVICE_ENABLE 0U',
    '<FilePath>\.\./App/Src/app_tilt_control\.c</FilePath>'
)) {
    $source = if ($required -match 'typedef struct|h723_jy901s_service_get_snapshot.*snapshot') { $jyHeader } `
              elseif ($required -eq 's_control_snapshot_sequence') { $jyService } `
              elseif ($required -match 'imu_|h723_jy901s_service_get_snapshot\\\(') { $chassisService } `
              elseif ($required -match 'APP_H723') { $config } `
              elseif ($required -match 'bno055') { $freertosSource } `
              else { $project }
    if ($source -notmatch $required) { throw "Missing JY901S tilt integration requirement: $required" }
}

if ($freertosSource -notmatch '#if \(APP_H723_BNO055_SERVICE_ENABLE == 1U\)[\s\S]*startBno055Task') {
    throw 'BNO055 task must be guarded by APP_H723_BNO055_SERVICE_ENABLE.'
}

if ($chassisService -match 'app_bno055_service\.h|h723_bno055_service_get_snapshot') {
    throw 'Tilt control must not reference the BNO055 service.'
}

foreach ($required in @(
    'derivative_filter_N',
    'measured_tilt_rate_deg_s',
    'filtered_tilt_rate_deg_s'
)) {
    if ($tiltHeader -notmatch [regex]::Escape($required)) {
        throw "Tilt-control advanced PID interface is missing: $required"
    }
}
foreach ($required in @(
    'pid->d_out\s*=\s*-params->kd\s*\*\s*pid->filtered_tilt_rate_deg_s',
    'capture_zero_request[\s\S]*app_tilt_pid_reset',
    's_tilt_control\.config\.derivative_filter_N\s*=\s*debug->derivative_filter_N'
)) {
    $source = if ($required -match 's_tilt_control') { $chassisService } else { $tiltService }
    if ($source -notmatch $required) {
        throw "Missing derivative-on-measurement control behavior: $required"
    }
}

Write-Output 'STM32H723 JY901S tilt integration static test passed.'
