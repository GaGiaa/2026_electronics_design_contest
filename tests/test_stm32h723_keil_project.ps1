[CmdletBinding()]
param(
    [string] $ProjectRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

$projectPath = Join-Path $ProjectRoot 'stm32h723_app\MDK-ARM\stm32h723_app.uvprojx'
if (-not (Test-Path -LiteralPath $projectPath)) {
    throw "Missing Keil project: $projectPath"
}

$project = Get-Content -LiteralPath $projectPath -Raw
foreach ($line in @(
    '<pCCUsed>6240000::V6.24::ARMCLANG</pCCUsed>',
    '<uAC6>1</uAC6>',
    '<PackID>Keil.STM32H7xx_DFP.4.1.2</PackID>',
    '<PackURL>https://www.keil.com/pack/</PackURL>',
    '<FilePath>../App/Src/app_debug.c</FilePath>',
    '<FilePath>../App/Src/app_buttons.c</FilePath>',
    '<FilePath>../App/Src/app_grayscale_math.c</FilePath>',
    '<FilePath>../App/Src/app_grayscale.c</FilePath>',
    '<FilePath>../App/Src/app_grayscale_service.c</FilePath>',
    '<FilePath>../App/Src/app_telemetry.c</FilePath>',
    '<FilePath>../App/Src/app_jy901s.c</FilePath>',
    '<FilePath>../App/Src/app_jy901s_calibration.c</FilePath>',
    '<FilePath>../App/Src/app_jy901s_service.c</FilePath>',
    '<FilePath>../App/Src/app_bno055.c</FilePath>',
    '<FilePath>../App/Src/app_bno055_service.c</FilePath>',
    '<FilePath>../App/Src/vofa_justfloat.c</FilePath>',
    '<FilePath>../App/Src/app_crsf.c</FilePath>',
    '<FilePath>../App/Src/app_m2006.c</FilePath>',
    '<FilePath>../App/Src/app_chassis.c</FilePath>',
    '<FilePath>../App/Src/app_chassis_service.c</FilePath>',
    '<FilePath>../App/Src/app_line_follow.c</FilePath>',
    '<FilePath>../App/Src/app_task_menu.c</FilePath>',
    '<FilePath>../App/Src/app_single_motor.c</FilePath>',
    '<FilePath>../../shared/pid/pid.c</FilePath>',
    '../../shared/pid'
)) {
    if ($project -notmatch [regex]::Escape($line)) {
        throw "Missing expected Keil configuration: $line"
    }
}

Write-Output 'STM32H723 Keil project configuration test passed.'
