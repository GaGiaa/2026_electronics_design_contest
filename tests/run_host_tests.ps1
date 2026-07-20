Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$visualStudioRoot = 'D:\Microsoft Visual Studio 2022'
$buildDirectory = Join-Path $PSScriptRoot 'build'
$testSource = Join-Path $PSScriptRoot 'test_echo_queue.c'
$queueSource = Join-Path $projectRoot 'app\echo_queue.c'
$testExecutable = Join-Path $buildDirectory 'test_echo_queue.exe'

if (-not (Test-Path -LiteralPath $visualStudioRoot)) {
    throw "Visual Studio root was not found: $visualStudioRoot"
}

$visualStudioPath = Get-ChildItem -LiteralPath $visualStudioRoot -Directory |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'VC\Auxiliary\Build\vcvars64.bat') } |
    Select-Object -First 1 -ExpandProperty FullName
$vcVars = Join-Path $visualStudioPath 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path -LiteralPath $vcVars)) {
    throw "Visual Studio C compiler environment was not found: $vcVars"
}

New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
$command = 'call "{0}" >nul && cl /nologo /W4 /WX /I"{1}\app" "{2}" "{3}" /Fe:"{4}"' -f `
    $vcVars, $projectRoot, $testSource, $queueSource, $testExecutable
cmd.exe /d /c $command
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $testExecutable
exit $LASTEXITCODE
