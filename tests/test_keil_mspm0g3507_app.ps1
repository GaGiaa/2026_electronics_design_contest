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

function Assert-Equal {
    param(
        [Parameter(Mandatory)] $Actual,
        [Parameter(Mandatory)] $Expected,
        [Parameter(Mandatory)] [string] $Description
    )

    if ($Actual -ne $Expected) {
        throw "${Description}: expected '$Expected', got '$Actual'"
    }
}

$projectDir = Join-Path $ProjectRoot 'keil\mspm0g3507_app'
$projectFile = Join-Path $projectDir 'mspm0g3507_app.uvprojx'
$buildScript = Join-Path $ProjectRoot 'tools\build-keil-mspm0g3507-app.ps1'
$generator = Join-Path $ProjectRoot 'tools\generate-keil-mspm0g3507-sysconfig.ps1'
$readme = Join-Path $projectDir 'README.md'
$portDir = Join-Path $projectDir 'freertos_port\ARM_CM0'
$scatter = Join-Path $projectDir 'mspm0g3507.sct'
$startup = Join-Path $projectDir 'startup_mspm0g350x_uvision.s'
$gitIgnore = Join-Path $ProjectRoot '.gitignore'
$handoff = Join-Path $ProjectRoot 'docs\AI_HANDOFF.md'
$packDir = 'D:\Keil_v5\ARM\PACK\TexasInstruments\MSPM0G1X0X_G3X0X_DFP\1.3.1'

foreach ($path in @(
        $projectDir, $projectFile, $buildScript, $generator, $readme, $portDir, $scatter,
        $startup, $gitIgnore, $handoff,
        (Join-Path $portDir 'port.c'),
        (Join-Path $portDir 'portasm.c'),
        (Join-Path $portDir 'portasm.h'),
        (Join-Path $portDir 'portmacro.h'),
        (Join-Path $packDir 'TexasInstruments.MSPM0G1X0X_G3X0X_DFP.pdsc'),
        (Join-Path $packDir '03_SVD\MSPM0G350X.svd'),
        (Join-Path $packDir '02_Flash_Programming\FlashARM\MSPM0G1X0X_G3X0X_MAIN_128KB.FLM'))) {
    Assert-Path -Path $path -Description 'Keil project is incomplete'
}

$xml = [xml](Get-Content -Raw -Encoding UTF8 -LiteralPath $projectFile)
$target = $xml.Project.Targets.Target
$common = $target.TargetOption.TargetCommonOption
$compiler = $target.TargetOption.TargetArmAds.Cads
$memory = $target.TargetOption.TargetArmAds.ArmAdsMisc.OnChipMemories

Assert-Equal -Actual $common.Device -Expected 'MSPM0G3507' -Description 'Keil device'
Assert-Equal -Actual $common.Vendor -Expected 'Texas Instruments' -Description 'Keil device vendor'
Assert-Equal -Actual $common.PackID -Expected 'TexasInstruments.MSPM0G1X0X_G3X0X_DFP.1.3.1' -Description 'CMSIS-Pack ID'
Assert-Contains -Path $projectFile -Pattern 'MSPM0G1X0X_G3X0X_MAIN_128KB\.FLM' -Description '128 KB main Flash algorithm'
Assert-Contains -Path $projectFile -Pattern '03_SVD\\MSPM0G350X\.svd' -Description 'MSPM0G350X SVD'
Assert-Equal -Actual $memory.IROM.Size -Expected '0x20000' -Description 'Flash size'
Assert-Equal -Actual $memory.OCR_RVCT9.StartAddress -Expected '0x20200000' -Description 'RAM address'
Assert-Equal -Actual $memory.OCR_RVCT9.Size -Expected '0x8000' -Description 'RAM size'
Assert-Equal -Actual $target.TargetOption.Utilities.Flash1.DriverSelection -Expected '4096' -Description 'CMSIS-DAP selection'
Assert-Equal -Actual $target.TargetOption.TargetArmAds.ArmAdsMisc.AdsCpuType -Expected '"Cortex-M0+"' -Description 'CPU core'
Assert-Equal -Actual $target.TargetOption.TargetArmAds.LDads.ScatterFile -Expected '.\mspm0g3507.sct' -Description 'scatter file'
Assert-Equal -Actual $target.TargetOption.TargetCommonOption.DebugInformation -Expected '1' -Description 'debug information'
Assert-Equal -Actual $compiler.vShortEn -Expected '1' -Description 'short enum ABI'
Assert-Equal -Actual $compiler.vShortWch -Expected '1' -Description 'short wchar ABI'
Assert-Contains -Path $projectFile -Pattern '\.\.\\\.\.\\mspm0g3507_app\\main\.c' -Description 'shared application source'
Assert-Contains -Path $projectFile -Pattern '\.\.\\\.\.\\mspm0g3507_app\\board_buttons\.c' -Description 'shared button driver source'
Assert-Contains -Path $projectFile -Pattern '\.\.\\\.\.\\mspm0g3507_app\\board_bmi160\.c' -Description 'shared BMI160 driver source'
Assert-Contains -Path $projectFile -Pattern '\.\.\\\.\.\\mspm0g3507_app\\board_grayscale\.c' -Description 'shared grayscale source'
Assert-Contains -Path $projectFile -Pattern '\.\.\\\.\.\\mspm0g3507_app\\line_tracking\.c' -Description 'Keil project must compile the line tracking layer'
Assert-Contains -Path $projectFile -Pattern '\.\.\\\.\.\\mspm0g3507_app\\motor_control\.c' -Description 'Keil project must compile the motor control layer'
Assert-Contains -Path $projectFile -Pattern '\.\.\\\.\.\\mspm0g3507_app\\encoder_quadrature\.c' -Description 'Keil project must compile the X4 quadrature decoder'
Assert-Contains -Path $projectFile -Pattern '\.\.\\\.\.\\mspm0g3507_app\\motor_pid\\pid\.c' -Description 'Keil project must compile the PID core'
Assert-Contains -Path $projectFile -Pattern '\.\.\\\.\.\\mspm0g3507_app\\vofa_justfloat\.c' -Description 'Keil project must compile the JustFloat encoder'
Assert-Contains -Path $projectFile -Pattern 'Generated\\ti_msp_dl_config\.c' -Description 'Keil SysConfig C output'
Assert-Contains -Path $projectFile -Pattern 'freertos_port\\ARM_CM0\\port\.c' -Description 'local Keil FreeRTOS port'
Assert-NotContains -Path $projectFile -Pattern 'TI_ARM_CLANG' -Description 'Keil project must not use the TI Clang port'
Assert-Contains -Path $generator -Pattern '--compiler keil' -Description 'Keil SysConfig compiler selection'
Assert-Contains -Path $buildScript -Pattern 'UV4\.exe' -Description 'Keil batch build invocation'
Assert-Contains -Path $buildScript -Pattern 'mspm0g3507_app\.uvprojx' -Description 'Keil project build target'
Assert-Contains -Path $buildScript -Pattern 'EncoderDecodeMode' -Description 'Keil build must support an explicit encoder decode mode'
Assert-Contains -Path $buildScript -Pattern 'VofaSpeedPidTelemetryEnable' -Description 'Keil build must support an explicit speed VOFA telemetry mode'
Assert-Contains -Path $buildScript -Pattern 'GrayVofaTelemetryEnable' -Description 'Keil build must support an explicit gray VOFA telemetry mode'
Assert-Contains -Path $buildScript -Pattern "PSBoundParameters\.ContainsKey\('VofaSpeedPidTelemetryEnable'\)" -Description 'Keil build must inject speed VOFA mode when explicitly requested'
Assert-Contains -Path $buildScript -Pattern 'VOFA_SPEED_PID_TELEMETRY_ENABLE=' -Description 'Keil build must inject the speed VOFA compiler define'
Assert-Contains -Path $buildScript -Pattern 'GRAY_VOFA_TELEMETRY_ENABLE=' -Description 'Keil build must inject the gray VOFA compiler define'
Assert-Contains -Path $readme -Pattern 'GRAY_VOFA_TELEMETRY_ENABLE' -Description 'gray VOFA telemetry documentation'
Assert-Contains -Path $readme -Pattern '22' -Description 'gray VOFA channel count documentation'
Assert-Contains -Path $readme -Pattern 'CMSIS-DAP' -Description 'debugger documentation'
Assert-Contains -Path $readme -Pattern '2dd0719d' -Description 'probe UID documentation'
Assert-Contains -Path $gitIgnore -Pattern '/keil/mspm0g3507_app/Generated/' -Description 'generated output ignore rule'
Assert-Contains -Path $gitIgnore -Pattern '/keil/mspm0g3507_app/Objects/' -Description 'Keil object output ignore rule'
Assert-Contains -Path $handoff -Pattern 'Keil MDK' -Description 'handoff migration record'

Write-Output 'MSPM0G3507 Keil static integration checks passed.'
