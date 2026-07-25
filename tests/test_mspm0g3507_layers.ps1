[CmdletBinding()]
param(
    [string] $ProjectRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

function Assert-Path {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Description
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "${Description}: missing $Path"
    }
}

function Assert-Contains {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Pattern,
        [Parameter(Mandatory)] [string] $Description
    )

    if (-not (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) {
        throw "${Description}: '$Pattern' was not found in $Path"
    }
}

function Assert-NotContains {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Pattern,
        [Parameter(Mandatory)] [string] $Description
    )

    if (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet) {
        throw "${Description}: '$Pattern' must not be present in $Path"
    }
}

$projectDir = Join-Path $ProjectRoot 'mspm0g3507_app'
$expectedDirectories = @(
    'app',
    'drivers\motor',
    'drivers\encoder',
    'drivers\imu',
    'drivers\grayscale',
    'drivers\uart',
    'drivers\crsf_uart',
    'drivers\oled',
    'drivers\buttons',
    'drivers\buzzer',
    'drivers\servo',
    'drivers\ws2812',
    'algorithms\pid',
    'algorithms\encoder',
    'algorithms\line_tracking',
    'algorithms\imu_yaw',
    'algorithms\motor_control',
    'protocols\crsf',
    'protocols\vofa',
    'services\rtos_monitor',
    'config',
    'platform'
)

foreach ($relativePath in $expectedDirectories) {
    Assert-Path -Path (Join-Path $projectDir $relativePath) -Description 'layer directory'
}

$main = Join-Path $projectDir 'main.c'
$startup = Join-Path $projectDir 'app\app_startup.c'
$interrupts = Join-Path $projectDir 'platform\g3507_interrupts.c'
$compatMotorHeader = Join-Path $projectDir 'board_motor.h'
$canonicalMotorHeader = Join-Path $projectDir 'drivers\motor\board_motor.h'
$canonicalPid = Join-Path $projectDir 'algorithms\pid\pid.c'

foreach ($path in @($main, $startup, $interrupts, $compatMotorHeader, $canonicalMotorHeader, $canonicalPid)) {
    Assert-Path -Path $path -Description 'layer migration file'
}

Assert-Contains -Path $main -Pattern '#include "app/app_startup\.h"' -Description 'main must call the application startup layer'
Assert-Contains -Path $main -Pattern 'app_startup\(\)' -Description 'main must invoke app_startup'
Assert-NotContains -Path $main -Pattern 'static void motor_task' -Description 'motor task must leave main'
Assert-NotContains -Path $main -Pattern 'UART_0_INST_IRQHandler' -Description 'UART interrupt must leave main'
Assert-Contains -Path $startup -Pattern 'app_tasks_motor_start\(\)' -Description 'startup must register motor tasks'
Assert-Contains -Path $interrupts -Pattern 'UART_0_INST_IRQHandler' -Description 'platform must own UART0 interrupt'
Assert-Contains -Path $interrupts -Pattern 'GROUP1_IRQHandler' -Description 'platform must own encoder group interrupt'
Assert-Contains -Path $compatMotorHeader -Pattern '#include "drivers/motor/board_motor\.h"' -Description 'root motor header must be a compatibility wrapper'
Assert-NotContains -Path $compatMotorHeader -Pattern 'board_motor_set_signed_duty' -Description 'compatibility header must not duplicate declarations'

$algorithmFiles = Get-ChildItem -LiteralPath (Join-Path $projectDir 'algorithms') -Recurse -File -Include '*.c', '*.h'
foreach ($file in $algorithmFiles) {
    Assert-NotContains -Path $file.FullName -Pattern 'ti_msp_dl_config\.h' -Description 'algorithm must not depend on generated hardware configuration'
}

Write-Output 'MSPM0G3507 layer structure checks passed.'
