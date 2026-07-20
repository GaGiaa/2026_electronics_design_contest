[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('List', 'Load', 'GdbServer')]
    [string] $Action
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$VenvPython = Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
$PackRoot = Join-Path $PSScriptRoot 'packs'
$PackFile = Join-Path $PackRoot 'TexasInstruments.MSPM0L11XX_L13XX_DFP.1.3.1.pack'
$ElfPath = Join-Path $ProjectRoot 'mspm0l1306_bringup\Debug\mspm0l1306_bringup.out'
$Target = 'MSPM0L1306'
$InstallScript = Join-Path $PSScriptRoot 'install-debug-tools.ps1'
$UserScript = Join-Path $PSScriptRoot 'pyocd-mspm0l1306.py'

function Invoke-PyOcd {
    param(
        [Parameter(Mandatory)] [string[]] $Arguments,
        [Parameter(Mandatory)] [string] $Description
    )

    & $VenvPython -m pyocd @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed (exit code $LASTEXITCODE). Check the DAPLink USB connection, SWD wiring, and target power."
    }
}

try {
    if (-not (Test-Path -LiteralPath $VenvPython)) {
        throw "pyOCD is not installed in '$VenvPython'. Run '$InstallScript' first."
    }

    if (-not (Test-Path -LiteralPath $PackFile) -or (Get-Item -LiteralPath $PackFile).Length -le 0) {
        throw "The CMSIS-Pack '$PackFile' is missing or empty. Run '$InstallScript' first."
    }

    switch ($Action) {
        'List' {
            Invoke-PyOcd -Arguments @('list', '--pack', $PackFile) -Description 'Listing CMSIS-DAP/DAPLink probes'
        }
        'Load' {
            if (-not (Test-Path -LiteralPath $UserScript)) {
                throw "pyOCD MSPM0L1306 compatibility script was not found: '$UserScript'."
            }
            if (-not (Test-Path -LiteralPath $ElfPath)) {
                throw "Firmware ELF was not found at '$ElfPath'. Build the CCS Debug configuration before loading firmware."
            }

            Write-Host "Loading '$ElfPath' to $Target..."
            Invoke-PyOcd -Arguments @('load', '--color', 'never', '--format', 'elf', '--script', $UserScript, '--pack', $PackFile, '--erase', 'chip', '--target', $Target, $ElfPath) -Description 'Erasing Flash and loading firmware'
        }
        'GdbServer' {
            if (-not (Test-Path -LiteralPath $UserScript)) {
                throw "pyOCD MSPM0L1306 compatibility script was not found: '$UserScript'."
            }
            Write-Host "Starting pyOCD GDB server for $Target on localhost:3333. This command does not program Flash."
            Invoke-PyOcd -Arguments @('gdbserver', '--color', 'never', '--script', $UserScript, '--pack', $PackFile, '--target', $Target, '--persist') -Description 'Starting the pyOCD GDB server'
        }
    }
}
catch {
    Write-Error "DAPLink action '$Action' failed: $($_.Exception.Message)"
    exit 1
}
