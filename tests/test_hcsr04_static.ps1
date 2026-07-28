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
    param([string] $Path, [string] $Pattern, [string] $Description)
    if (-not [regex]::IsMatch((Get-Content -Raw -LiteralPath $Path), $Pattern)) {
        throw "${Description}: '$Pattern' was not found in $Path."
    }
}

function Assert-OrderedPatterns {
    param([string] $Path, [string[]] $Patterns, [string] $Description)
    $content = Get-Content -Raw -LiteralPath $Path
    $offset = 0
    foreach ($pattern in $Patterns) {
        $match = [regex]::Match($content, $pattern, [System.Text.RegularExpressions.RegexOptions]::Multiline,
                                [TimeSpan]::FromSeconds(1))
        if (-not $match.Success -or $match.Index -lt $offset) {
            throw "${Description}: ordered pattern '$pattern' was not found in $Path."
        }
        $offset = $match.Index + $match.Length
    }
}

$projectDir = Join-Path $ProjectRoot 'mspm0g3507_app'
$syscfg = Join-Path $projectDir 'mspm0g3507_app.syscfg'
$driver = Join-Path $projectDir 'drivers\hcsr04\board_hcsr04.c'
$interrupts = Join-Path $projectDir 'platform\g3507_interrupts.c'
$config = Join-Path $projectDir 'config\app_config.h'
$build = Join-Path $ProjectRoot 'tools\build-mspm0g3507-app.ps1'

Assert-Contains $syscfg 'GPIO5.\$name = "HCSR04"' 'HC-SR04 GPIO instance must exist'
Assert-Contains $syscfg 'pin.\$assign = "PB10"' 'TRIG must use PB10'
Assert-Contains $syscfg 'pin.\$assign = "PB11"' 'ECHO must use PB11'
Assert-Contains $syscfg 'polarity = "RISE_FALL"' 'ECHO must use both edges'
Assert-Contains $driver 'rtos_monitor_runtime_timer_now' 'Driver must use the shared timer'
Assert-Contains $driver 'vTaskNotifyGiveFromISR' 'Echo completion must notify the task'
Assert-Contains $driver 'board_hcsr04_abort' 'Driver must expose an explicit timeout disarm operation'
Assert-OrderedPatterns $driver @(
    'DL_GPIO_disableInterrupt\(HCSR04_PORT, HCSR04_ECHO_PIN\)',
    'DL_GPIO_clearInterruptStatus\(HCSR04_PORT, HCSR04_ECHO_PIN\)',
    'DL_GPIO_enableInterrupt\(HCSR04_PORT, HCSR04_ECHO_PIN\)'
) 'Trigger must clear stale Echo status before arming the next measurement'
Assert-Contains -Path (Join-Path $projectDir 'app\app_tasks_sensor.c') -Pattern 'board_hcsr04_abort' -Description 'HC-SR04 task must disarm Echo interrupts after timeout'
Assert-Contains $interrupts 'board_hcsr04_gpio_irq_handler' 'GPIOB IRQ must dispatch HC-SR04'
Assert-Contains $config 'APP_HCSR04_ENABLE[ 	]+0U' 'HC-SR04 must default to disabled'
Assert-Contains $build 'board_hcsr04.c' 'TI build must include HC-SR04 driver'

Write-Output 'HC-SR04 static integration checks passed.'
