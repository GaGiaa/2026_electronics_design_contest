$ErrorActionPreference = 'Stop'

$wrapper = Get-Content -Raw (Join-Path $PSScriptRoot '..\tools\flash-and-debug.ps1')
if ($wrapper -notmatch 'pyocd load --color never --format elf') {
    throw 'Load must explicitly pass --format elf for TI Clang .out files.'
}
