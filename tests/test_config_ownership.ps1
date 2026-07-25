[CmdletBinding()]
param(
    [string] $ProjectRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

function Assert-Contains {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Pattern,
        [Parameter(Mandatory)] [string] $Description
    )

    if (-not (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) {
        throw "${Description}: '$Pattern' was not found in $Path."
    }
}

function Assert-NotContains {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Pattern,
        [Parameter(Mandatory)] [string] $Description
    )

    if (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet) {
        throw "${Description}: '$Pattern' must not be present in $Path."
    }
}

$projectDir = Join-Path $ProjectRoot 'mspm0g3507_app'
$profile = Join-Path $projectDir 'app\app_profile.h'
$appConfig = Join-Path $projectDir 'config\app_config.h'
$encoderConfig = Join-Path $projectDir 'config\encoder_config.h'
$encoderHeader = Join-Path $projectDir 'drivers\encoder\board_encoder.h'
$monitorConfig = Join-Path $projectDir 'config\rtos_monitor_config.h'
$monitorHeader = Join-Path $projectDir 'services\rtos_monitor\rtos_monitor.h'
$freertosConfig = Join-Path $projectDir 'config\FreeRTOSConfig.h'

foreach ($path in @($profile, $appConfig, $encoderConfig, $encoderHeader,
                    $monitorConfig, $monitorHeader, $freertosConfig)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required configuration file is missing: $path"
    }
}

Assert-Contains -Path $appConfig -Pattern '#define APP_IMU_YAW_ENABLE\s+1U' `
    -Description 'Application configuration must own the prefixed IMU yaw switch'
Assert-Contains -Path $appConfig -Pattern '#define APP_BUZZER_FEATURE_ENABLE\s+0U' `
    -Description 'Application configuration must own the prefixed buzzer switch'
Assert-NotContains -Path $appConfig -Pattern '#define\s+IMU_YAW_ENABLE\b' `
    -Description 'Application configuration must not define the old IMU yaw switch'
Assert-NotContains -Path $profile -Pattern '^\s*#\s*define\s+APP_(?!PROFILE_H\b)' `
    -Description 'Application profile must not own configuration macros'

Assert-Contains -Path $encoderConfig -Pattern '#define BOARD_ENCODER_MOTOR_LINES_PER_REVOLUTION\s+13U' `
    -Description 'Encoder mechanics must live in encoder configuration'
Assert-Contains -Path $encoderHeader -Pattern '#include "config/encoder_config\.h"' `
    -Description 'Encoder interface must include encoder configuration'
Assert-NotContains -Path $encoderHeader -Pattern '^\s*#\s*define\s+BOARD_ENCODER_MOTOR_LINES_PER_REVOLUTION' `
    -Description 'Encoder interface must not define encoder mechanics'

Assert-Contains -Path $monitorConfig -Pattern '#define RTOS_MONITOR_TASK_NAME_LENGTH\s+16U' `
    -Description 'Monitor configuration must own task name length'
Assert-Contains -Path $monitorHeader -Pattern '#include "config/rtos_monitor_config\.h"' `
    -Description 'Monitor interface must include monitor configuration'
Assert-NotContains -Path $monitorHeader -Pattern '#define\s+configMAX_TASK_NAME_LEN' `
    -Description 'Monitor interface must not define the FreeRTOS kernel macro'
Assert-Contains -Path $freertosConfig -Pattern 'configMAX_TASK_NAME_LEN\s+RTOS_MONITOR_TASK_NAME_LENGTH' `
    -Description 'FreeRTOS task name length must derive from monitor configuration'

Write-Output 'Configuration ownership checks passed.'
