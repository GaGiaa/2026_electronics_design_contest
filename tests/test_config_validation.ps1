[CmdletBinding()]
param(
    [string] $ProjectRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$includeRoot = Join-Path $ProjectRoot 'mspm0g3507_app'

function Assert-ConfigCompileFails {
    param(
        [Parameter(Mandatory)] [string] $Header,
        [Parameter(Mandatory)] [string[]] $Defines,
        [Parameter(Mandatory)] [string] $Description
    )

    $source = "#include `"$Header`"`nint main(void) { return 0; }`n"
    $arguments = @('-std=c11', '-Wall', '-Werror', '-fsyntax-only', '-x', 'c', "-I$includeRoot")
    $arguments += $Defines
    $arguments += '-'
    $previousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $output = $source | & $gcc @arguments 2>&1
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorAction
    if ($exitCode -eq 0) {
        throw "Invalid configuration unexpectedly compiled: $Description"
    }
}

Assert-ConfigCompileFails -Header 'config/app_config.h' `
    -Defines @('-DAPP_BUZZER_DUTY_PERCENT=101U') `
    -Description 'buzzer duty over 100 percent'
Assert-ConfigCompileFails -Header 'config/app_config.h' `
    -Defines @('-DAPP_SERVO_MAX_PULSE_US=20000U') `
    -Description 'servo pulse outside 20 ms period'
Assert-ConfigCompileFails -Header 'config/app_config.h' `
    -Defines @('-DAPP_MOTOR_TASK_STACK_DEPTH=0U') `
    -Description 'motor task stack depth is zero'
Assert-ConfigCompileFails -Header 'config/app_config.h' `
    -Defines @('-DAPP_SERVO_TASK_INTERVAL_MS=0U') `
    -Description 'servo task period is zero'
Assert-ConfigCompileFails -Header 'config/app_config.h' `
    -Defines @('-DAPP_IMU_VOFA_TELEMETRY_TASK_STACK_DEPTH=0U') `
    -Description 'IMU VOFA task stack depth is zero'
Assert-ConfigCompileFails -Header 'config/app_config.h' `
    -Defines @('-DAPP_IMU_VOFA_TELEMETRY_INTERVAL_MS=0U') `
    -Description 'IMU VOFA telemetry period is zero'
Assert-ConfigCompileFails -Header 'config/app_config.h' `
    -Defines @('-DAPP_IMU_TELEMETRY_ENABLE=1U', '-DAPP_IMU_YAW_ENABLE=0U') `
    -Description 'IMU telemetry is enabled without yaw'
Assert-ConfigCompileFails -Header 'config/app_config.h' `
    -Defines @('-DAPP_VOFA_SPEED_PID_TELEMETRY_ENABLE=1U',
               '-DAPP_IMU_TELEMETRY_ENABLE=1U', '-DAPP_IMU_YAW_ENABLE=1U') `
    -Description 'conflicting telemetry modes'
Assert-ConfigCompileFails -Header 'config/encoder_config.h' `
    -Defines @('-DBOARD_ENCODER_DECODE_MODE=99U') `
    -Description 'unsupported encoder decode mode'
Assert-ConfigCompileFails -Header 'config/encoder_config.h' `
    -Defines @('-DBOARD_ENCODER_SAMPLE_PERIOD_MS=0U') `
    -Description 'encoder sample period is zero'
Assert-ConfigCompileFails -Header 'config/rtos_monitor_config.h' `
    -Defines @('-DRTOS_MONITOR_TASK_NAME_LENGTH=1U') `
    -Description 'RTOS monitor task name capacity below two bytes'
Assert-ConfigCompileFails -Header 'config/crsf_config.h' `
    -Defines @('-DCRSF_FORWARD_CHANNEL_INDEX=16U') `
    -Description 'CRSF channel index outside packed channel array'
Assert-ConfigCompileFails -Header 'config/crsf_config.h' `
    -Defines @('-DCRSF_CHANNEL_CENTER=1811U') `
    -Description 'CRSF channel range is not increasing'

$global:LASTEXITCODE = 0
Write-Output 'Configuration validation tests passed.'
