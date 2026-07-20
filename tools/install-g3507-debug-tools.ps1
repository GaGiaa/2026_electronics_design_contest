[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$VenvPath = Join-Path $PSScriptRoot '.venv'
$VenvPython = Join-Path $VenvPath 'Scripts\python.exe'
$PackRoot = Join-Path $PSScriptRoot 'packs'
$PackId = 'TexasInstruments.MSPM0G1X0X_G3X0X_DFP'
$PackFile = Join-Path $PackRoot 'TexasInstruments.MSPM0G1X0X_G3X0X_DFP.1.3.1.pack'
$PackUri = 'https://software-dl.ti.com/msp430/esd/MSPM0-CMSIS/MSPM0G1X0X_G3X0X/latest/exports/TexasInstruments.MSPM0G1X0X_G3X0X_DFP.1.3.1.pack'

function Invoke-CheckedCommand {
    param(
        [Parameter(Mandatory)] [string] $FilePath,
        [Parameter(Mandatory)] [string[]] $Arguments,
        [Parameter(Mandatory)] [string] $Description
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed (exit code $LASTEXITCODE)."
    }
}

try {
    $PythonLauncher = Get-Command py -ErrorAction SilentlyContinue
    $PythonExecutable = Get-Command python -ErrorAction SilentlyContinue

    if (-not $PythonLauncher -and -not $PythonExecutable) {
        throw 'Python was not found. Install Python 3 with the Python launcher or add python.exe to PATH.'
    }

    if (-not (Test-Path -LiteralPath $VenvPython)) {
        if ($PythonLauncher) {
            Invoke-CheckedCommand -FilePath $PythonLauncher.Source -Arguments @('-3', '-m', 'venv', $VenvPath) -Description 'Creating the Python virtual environment'
        }
        else {
            Invoke-CheckedCommand -FilePath $PythonExecutable.Source -Arguments @('-m', 'venv', $VenvPath) -Description 'Creating the Python virtual environment'
        }
    }

    New-Item -ItemType Directory -Force -Path $PackRoot | Out-Null
    Invoke-CheckedCommand -FilePath $VenvPython -Arguments @('-m', 'pip', 'install', '--upgrade', 'pyocd>=0.45,<0.46') -Description 'Installing pyOCD'
    Invoke-WebRequest -Uri $PackUri -OutFile $PackFile

    $AvailableTargets = & $VenvPython -m pyocd list --pack $PackFile --targets 2>&1
    if ($LASTEXITCODE -ne 0 -or -not ($AvailableTargets -match '(?i)\bMSPM0G3507\b')) {
        throw "MSPM0G3507 is unavailable in '$PackFile'."
    }
    $AvailableTargets | Write-Output
}
catch {
    Write-Error "G3507 DAPLink tool setup failed: $($_.Exception.Message)"
    exit 1
}
