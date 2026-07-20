[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$VenvPath = Join-Path $PSScriptRoot '.venv'
$VenvPython = Join-Path $VenvPath 'Scripts\python.exe'
$PackRoot = Join-Path $PSScriptRoot 'packs'
$PackId = 'TexasInstruments.MSPM0L11XX_L13XX_DFP'
$PackFile = Join-Path $PackRoot 'TexasInstruments.MSPM0L11XX_L13XX_DFP.1.3.1.pack'
$PackUri = 'https://software-dl.ti.com/msp430/esd/MSPM0-CMSIS/MSPM0L11XX_L13XX/latest/exports/TexasInstruments.MSPM0L11XX_L13XX_DFP.1.3.1.pack'

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
        throw 'Python was not found. Install Python 3 with the Python launcher (py) or add python.exe to PATH, then run this script again.'
    }

    if (-not (Test-Path -LiteralPath $VenvPython)) {
        Write-Host "Creating local virtual environment: $VenvPath"
        if ($PythonLauncher) {
            Invoke-CheckedCommand -FilePath $PythonLauncher.Source -Arguments @('-3', '-m', 'venv', $VenvPath) -Description 'Creating the Python virtual environment'
        }
        else {
            Invoke-CheckedCommand -FilePath $PythonExecutable.Source -Arguments @('-m', 'venv', $VenvPath) -Description 'Creating the Python virtual environment'
        }
    }

    if (-not (Test-Path -LiteralPath $VenvPython)) {
        throw "The virtual environment was not created at '$VenvPython'. Verify that the selected Python installation includes the venv module."
    }

    New-Item -ItemType Directory -Force -Path $PackRoot | Out-Null

    Write-Host 'Upgrading pip in the local virtual environment...'
    Invoke-CheckedCommand -FilePath $VenvPython -Arguments @('-m', 'pip', 'install', '--upgrade', 'pip') -Description 'Upgrading pip'

    Write-Host 'Installing pyOCD (>=0.45,<0.46)...'
    Invoke-CheckedCommand -FilePath $VenvPython -Arguments @('-m', 'pip', 'install', '--upgrade', 'pyocd>=0.45,<0.46') -Description 'Installing pyOCD'

    Write-Host "Downloading CMSIS-Pack $PackId to $PackFile..."
    try {
        Invoke-WebRequest -Uri $PackUri -OutFile $PackFile -ErrorAction Stop
    }
    catch {
        throw "Downloading CMSIS-Pack '$PackId' from '$PackUri' failed. Check network access or download the file manually to '$PackFile'. Details: $($_.Exception.Message)"
    }

    if (-not (Test-Path -LiteralPath $PackFile) -or (Get-Item -LiteralPath $PackFile).Length -le 0) {
        throw "The downloaded CMSIS-Pack is missing or empty at '$PackFile'. Delete it if present, then run this script again."
    }

    $AvailableTargets = & $VenvPython -m pyocd list --pack $PackFile --targets 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Verifying the MSPM0L1306 target failed (exit code $LASTEXITCODE)."
    }
    $AvailableTargets | Write-Output
    if (-not ($AvailableTargets -match '(?i)\bMSPM0L1306\b')) {
        throw "MSPM0L1306 is unavailable in '$PackFile'. Verify the downloaded TI CMSIS-Pack version, then run this script again."
    }

    Write-Host 'DAPLink debugging tools are ready.'
}
catch {
    Write-Error "DAPLink tool setup failed: $($_.Exception.Message)"
    exit 1
}
