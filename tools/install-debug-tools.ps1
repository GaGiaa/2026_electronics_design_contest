Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$venvPath = Join-Path $projectRoot '.venv'
$python = Join-Path $venvPath 'Scripts\python.exe'
$packDirectory = Join-Path $PSScriptRoot 'packs'
$packPath = Join-Path $packDirectory 'TexasInstruments.MSPM0L11XX_L13XX_DFP.1.3.1.pack'
$packUri = 'https://software-dl.ti.com/msp430/esd/MSPM0-CMSIS/MSPM0L11XX_L13XX/latest/exports/TexasInstruments.MSPM0L11XX_L13XX_DFP.1.3.1.pack'

if (-not (Test-Path -LiteralPath $python)) {
    py -3 -m venv $venvPath
}

& $python -m pip install --upgrade pip pyocd

if (-not (Test-Path -LiteralPath $packPath)) {
    New-Item -ItemType Directory -Force -Path $packDirectory | Out-Null
    Invoke-WebRequest -Uri $packUri -OutFile $packPath
}
