[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$VenvPython = Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
$PackFile = Join-Path $PSScriptRoot 'packs\TexasInstruments.MSPM0G1X0X_G3X0X_DFP.1.3.1.pack'
$Target = 'MSPM0G3507'
$ProbeUid = '2dd0719d'
$GdbPort = 3333
$TelnetPort = 4444

foreach ($path in @($VenvPython, $PackFile)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required pyOCD debug path is unavailable: $path"
    }
}

$existing = Get-NetTCPConnection -LocalPort $GdbPort -State Listen -ErrorAction SilentlyContinue
if ($existing) {
    throw "GDB port $GdbPort is already in use by process $($existing.OwningProcess). Stop the existing debug server first."
}

$arguments = @(
    '-m', 'pyocd', 'gdbserver',
    '-v',
    '--color', 'never',
    '--port', $GdbPort.ToString(),
    '--telnet-port', $TelnetPort.ToString(),
    '--target', $Target,
    '--pack', $PackFile,
    '--uid', $ProbeUid,
    '--frequency', '1000000',
    '--persist'
)

Write-Host "Starting pyOCD GDB server for $Target using probe $ProbeUid..."
& $VenvPython @arguments
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    throw "pyOCD GDB server exited with code $exitCode."
}
