param(
    [ValidateSet('List', 'Load', 'GdbServer')]
    [string] $Action = 'List'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$python = Join-Path $projectRoot '.venv\Scripts\python.exe'
$packPath = Join-Path $PSScriptRoot 'packs\TexasInstruments.MSPM0L11XX_L13XX_DFP.1.3.1.pack'
$userScript = Join-Path $PSScriptRoot 'pyocd-mspm0l1306.py'
$elfPath = Join-Path $projectRoot 'ticlang\mspm0l1306_bringup.out'

if (-not (Test-Path -LiteralPath $python)) {
    throw 'pyOCD is not installed. Run tools\install-debug-tools.ps1 first.'
}
if (-not (Test-Path -LiteralPath $packPath)) {
    throw 'MSPM0 CMSIS-Pack is not installed. Run tools\install-debug-tools.ps1 first.'
}
switch ($Action) {
    'List' {
        & $python -m pyocd list --probes
    }
    'Load' {
        if (-not (Test-Path -LiteralPath $userScript)) {
            throw "pyOCD MSPM0L1306 compatibility script was not found: $userScript"
        }
        if (-not (Test-Path -LiteralPath $elfPath)) {
            throw "Firmware ELF was not found: $elfPath"
        }
        & $python -m pyocd load --color never --format elf --script $userScript --pack $packPath --target MSPM0L1306 $elfPath
    }
    'GdbServer' {
        if (-not (Test-Path -LiteralPath $userScript)) {
            throw "pyOCD MSPM0L1306 compatibility script was not found: $userScript"
        }
        & $python -m pyocd gdbserver --color never --script $userScript --pack $packPath --target MSPM0L1306 --persist
    }
}

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
