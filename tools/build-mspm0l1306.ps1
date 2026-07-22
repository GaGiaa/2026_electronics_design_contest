[CmdletBinding()]
param(
    [string] $CcsRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot 'mspm0l1306_bringup\Debug'
. (Join-Path $PSScriptRoot 'toolchain.ps1')
$toolchain = Get-ToolchainConfig -CcsRoot $CcsRoot
Assert-ToolchainConfig -Config $toolchain -Required @('Gmake')

if (-not (Test-Path -LiteralPath $BuildDir -PathType Container)) {
    throw "CCS build directory is missing: $BuildDir. Import the CCS project and generate its Debug configuration first."
}

Push-Location $BuildDir
try {
    & $toolchain.Gmake '-C' $BuildDir 'all'
    if ($LASTEXITCODE -ne 0) {
        throw "MSPM0L1306 build failed (exit code $LASTEXITCODE)."
    }
}
finally {
    Pop-Location
}

Write-Output "MSPM0L1306 build succeeded: $(Join-Path $BuildDir 'mspm0l1306_bringup.out')"
