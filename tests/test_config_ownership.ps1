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
$motorConfig = Join-Path $projectDir 'config\motor_config.h'
$motorHeader = Join-Path $projectDir 'drivers\motor\board_motor.h'
$monitorConfig = Join-Path $projectDir 'config\rtos_monitor_config.h'
$monitorHeader = Join-Path $projectDir 'services\rtos_monitor\rtos_monitor.h'
$freertosConfig = Join-Path $projectDir 'config\FreeRTOSConfig.h'

foreach ($path in @($profile, $appConfig, $encoderConfig, $encoderHeader,
                    $motorConfig, $motorHeader,
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
Assert-Contains -Path $encoderConfig -Pattern '#define BOARD_ENCODER_FRONT_LEFT_DIRECTION_SIGN\s+-1' `
    -Description 'Front-left encoder direction must be configurable'
Assert-Contains -Path $encoderConfig -Pattern '#define BOARD_ENCODER_FRONT_RIGHT_DIRECTION_SIGN\s+1' `
    -Description 'Front-right encoder direction must be configurable'
Assert-Contains -Path $encoderConfig -Pattern '#define BOARD_ENCODER_REAR_LEFT_DIRECTION_SIGN\s+1' `
    -Description 'Rear-left encoder direction must be configurable'
Assert-Contains -Path $encoderConfig -Pattern '#define BOARD_ENCODER_REAR_RIGHT_DIRECTION_SIGN\s+-1' `
    -Description 'Rear-right encoder direction must be configurable'
Assert-Contains -Path $encoderHeader -Pattern '#include "config/encoder_config\.h"' `
    -Description 'Encoder interface must include encoder configuration'
Assert-NotContains -Path $encoderHeader -Pattern '^\s*#\s*define\s+BOARD_ENCODER_MOTOR_LINES_PER_REVOLUTION' `
    -Description 'Encoder interface must not define encoder mechanics'

Assert-Contains -Path $motorConfig -Pattern '#define BOARD_MOTOR_FRONT_LEFT_DIRECTION_SIGN\s+-1' `
    -Description 'Motor polarity must live in motor configuration'
Assert-Contains -Path $motorConfig -Pattern '#define BOARD_MOTOR_FRONT_RIGHT_DIRECTION_SIGN\s+-1' `
    -Description 'Motor polarity must configure front-right independently'
Assert-Contains -Path $motorConfig -Pattern '#define BOARD_MOTOR_REAR_LEFT_DIRECTION_SIGN\s+-1' `
    -Description 'Motor polarity must preserve the rear-left inversion'
Assert-Contains -Path $motorConfig -Pattern '#define BOARD_MOTOR_REAR_RIGHT_DIRECTION_SIGN\s+1' `
    -Description 'Motor polarity must configure rear-right independently'
Assert-Contains -Path $motorHeader -Pattern '#include "config/motor_config\.h"' `
    -Description 'Motor interface must include motor configuration'
Assert-Contains -Path (Join-Path $projectDir 'drivers\motor\board_motor.c') `
    -Pattern 'BOARD_MOTOR_FRONT_LEFT_DIRECTION_SIGN' `
    -Description 'Motor driver must use the configured front-left polarity'
Assert-Contains -Path (Join-Path $projectDir 'drivers\motor\board_motor.c') `
    -Pattern 'BOARD_MOTOR_FRONT_RIGHT_DIRECTION_SIGN' `
    -Description 'Motor driver must use the configured front-right polarity'

$encoder = Join-Path $projectDir 'drivers\encoder\board_encoder.c'
Assert-Contains -Path $encoder -Pattern 'BOARD_ENCODER_FRONT_LEFT_DIRECTION_SIGN' `
    -Description 'Encoder driver must use the configured front-left direction'
Assert-Contains -Path $encoder -Pattern 'BOARD_ENCODER_FRONT_RIGHT_DIRECTION_SIGN' `
    -Description 'Encoder driver must use the configured front-right direction'
Assert-Contains -Path $encoder -Pattern 'BOARD_ENCODER_REAR_LEFT_DIRECTION_SIGN' `
    -Description 'Encoder driver must use the configured rear-left direction'
Assert-Contains -Path $encoder -Pattern 'BOARD_ENCODER_REAR_RIGHT_DIRECTION_SIGN' `
    -Description 'Encoder driver must use the configured rear-right direction'

Assert-Contains -Path $monitorConfig -Pattern '#define RTOS_MONITOR_TASK_NAME_LENGTH\s+16U' `
    -Description 'Monitor configuration must own task name length'
Assert-Contains -Path $monitorHeader -Pattern '#include "config/rtos_monitor_config\.h"' `
    -Description 'Monitor interface must include monitor configuration'
Assert-NotContains -Path $monitorHeader -Pattern '#define\s+configMAX_TASK_NAME_LEN' `
    -Description 'Monitor interface must not define the FreeRTOS kernel macro'
Assert-Contains -Path $freertosConfig -Pattern 'configMAX_TASK_NAME_LEN\s+RTOS_MONITOR_TASK_NAME_LENGTH' `
    -Description 'FreeRTOS task name length must derive from monitor configuration'

Write-Output 'Configuration ownership checks passed.'
