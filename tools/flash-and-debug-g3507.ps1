[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('List', 'Load', 'GdbServer')]
    [string] $Action,

    [ValidateSet('bringup', 'freertos', 'app')]
    [string] $Firmware = 'bringup'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$VenvPython = Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
$PackFile = Join-Path $PSScriptRoot 'packs\TexasInstruments.MSPM0G1X0X_G3X0X_DFP.1.3.1.pack'
$ElfPath = if ($Firmware -eq 'freertos') {
    Join-Path $ProjectRoot 'mspm0g3507_freertos\Debug\mspm0g3507_freertos.out'
}
elseif ($Firmware -eq 'app') {
    Join-Path $ProjectRoot 'mspm0g3507_app\Debug\mspm0g3507_app.out'
}
else {
    Join-Path $ProjectRoot 'mspm0g3507_bringup\Debug\mspm0g3507_bringup.out'
}
$Target = 'MSPM0G3507'
$InstallScript = Join-Path $PSScriptRoot 'install-g3507-debug-tools.ps1'

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
    if (-not (Test-Path -LiteralPath $VenvPython) -or -not (Test-Path -LiteralPath $PackFile)) {
        throw "G3507 pyOCD dependencies are unavailable. Run '$InstallScript' first."
    }

    switch ($Action) {
        'List' {
            Invoke-PyOcd -Arguments @('list', '--pack', $PackFile) -Description 'Listing CMSIS-DAP/DAPLink probes'
        }
        'Load' {
            if (-not (Test-Path -LiteralPath $ElfPath)) {
                throw "Firmware ELF was not found at '$ElfPath'. Build the selected G3507 firmware first."
            }

            Invoke-PyOcd -Arguments @('load', '--color', 'never', '--format', 'elf', '--pack', $PackFile, '--erase', 'chip', '--target', $Target, $ElfPath) -Description 'Erasing Flash and loading firmware'
        }
        'GdbServer' {
            Invoke-PyOcd -Arguments @('gdbserver', '--color', 'never', '--pack', $PackFile, '--target', $Target, '--persist') -Description 'Starting the pyOCD GDB server'
        }
    }
}
catch {
    Write-Error "G3507 DAPLink action '$Action' failed: $($_.Exception.Message)"
    exit 1
}
