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

    if (-not (Select-String -Path $Path -Pattern $Pattern -Quiet)) {
        throw "${Description}: '$Pattern' was not found in $Path."
    }
}

function Assert-NotContains {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Pattern,
        [Parameter(Mandatory)] [string] $Description
    )

    if (Select-String -Path $Path -Pattern $Pattern -Quiet) {
        throw "${Description}: '$Pattern' must not be present in $Path."
    }
}

$projectDir = Join-Path $ProjectRoot 'mspm0g3507_app'
$syscfg = Join-Path $projectDir 'mspm0g3507_app.syscfg'
$config = Join-Path $projectDir 'config\app_config.h'
$main = Join-Path $projectDir 'app\app_tasks_io.c'
$oledHeader = Join-Path $projectDir 'drivers\oled\board_oled.h'
$oledSource = Join-Path $projectDir 'drivers\oled\board_oled.c'
$font = Join-Path $projectDir 'drivers\oled\board_oled_font.c'
$buildScript = Join-Path $ProjectRoot 'tools\build-mspm0g3507-app.ps1'
$keilProject = Join-Path $ProjectRoot 'keil\mspm0g3507_app\mspm0g3507_app.uvprojx'

foreach ($path in @($syscfg, $config, $main, $oledHeader, $oledSource, $font, $buildScript, $keilProject)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "OLED integration file is missing: $path"
    }
}

Assert-Contains -Path $syscfg -Pattern 'const I2C = scripting\.addModule\("/ti/driverlib/I2C"' -Description 'SysConfig must add the I2C module'
Assert-Contains -Path $syscfg -Pattern 'I2C1\.peripheral\.\$assign\s*=\s*"I2C0"' -Description 'OLED must use I2C0'
Assert-Contains -Path $syscfg -Pattern 'I2C1\.peripheral\.sdaPin\.\$assign\s*=\s*"PA0"' -Description 'OLED SDA must use PA0'
Assert-Contains -Path $syscfg -Pattern 'I2C1\.peripheral\.sclPin\.\$assign\s*=\s*"PA1"' -Description 'OLED SCL must use PA1'
Assert-Contains -Path $syscfg -Pattern 'I2C1\.basicEnableController\s*=\s*true' -Description 'I2C controller mode must be enabled'

Assert-Contains -Path $config -Pattern 'OLED_TEST_TASK_ENABLE\s+0U' -Description 'OLED test task must default off'
Assert-Contains -Path $config -Pattern 'OLED_I2C_ADDRESS\s+0x3CU' -Description 'OLED address must default to 0x3C'
Assert-Contains -Path $main -Pattern '#include "drivers/oled/board_oled\.h"' -Description 'Application must include the OLED driver'
Assert-Contains -Path $main -Pattern 'OLED_TEST_TASK_ENABLE' -Description 'OLED task must be compile-time gated'
Assert-Contains -Path $main -Pattern 'xTaskCreateStatic\(oled_test_task' -Description 'OLED test task must use static allocation'
Assert-Contains -Path $main -Pattern 'board_oled_init' -Description 'Application must initialize OLED'

Assert-Contains -Path $oledHeader -Pattern 'board_oled_init' -Description 'OLED header must expose initialization'
Assert-Contains -Path $oledHeader -Pattern 'board_oled_write_string' -Description 'OLED header must expose string rendering'
Assert-Contains -Path $oledSource -Pattern 'DL_I2C_startControllerTransfer' -Description 'OLED driver must use hardware I2C transfers'
Assert-Contains -Path $oledSource -Pattern 'DL_I2C_isControllerTXFIFOFull' -Description 'OLED driver must refill the I2C TX FIFO'
Assert-Contains -Path $oledSource -Pattern 'BOARD_OLED_STATUS_TIMEOUT' -Description 'OLED driver must expose transfer timeout status'
Assert-Contains -Path $font -Pattern '0x20U' -Description 'OLED font must include printable ASCII space'
Assert-Contains -Path $font -Pattern '0x7EU' -Description 'OLED font must include printable ASCII tilde'
Assert-Contains -Path $font -Pattern 'g_board_oled_font' -Description 'OLED font table must be linked into the driver'

Assert-Contains -Path $buildScript -Pattern "board_oled\.c" -Description 'CCS build must compile the OLED driver'
Assert-Contains -Path $buildScript -Pattern "board_oled_font\.c" -Description 'CCS build must compile the OLED font'
Assert-Contains -Path $keilProject -Pattern '<FileName>board_oled\.c</FileName>' -Description 'Keil project must include the OLED driver'
Assert-Contains -Path $keilProject -Pattern '<FileName>board_oled_font\.c</FileName>' -Description 'Keil project must include the OLED font'

Write-Output 'MSPM0G3507 OLED static integration checks passed.'
