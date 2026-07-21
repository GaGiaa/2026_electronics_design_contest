[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$Gdb = 'D:\Software\STM32CubeCLT_1.18.0\GNU-tools-for-STM32\bin\arm-none-eabi-gdb.exe'
$port = Get-NetTCPConnection -LocalPort 3333 -State Listen -ErrorAction SilentlyContinue

if ($port -and (Test-Path -LiteralPath $Gdb)) {
    $previousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $Gdb -q -batch `
        -ex 'set pagination off' `
        -ex 'target extended-remote localhost:3333' `
        -ex 'monitor exit' `
        -ex 'disconnect' `
        -ex 'quit' 2>&1 | Out-Null
    $ErrorActionPreference = $previousErrorAction
}

Start-Sleep -Milliseconds 500
$remaining = @(Get-CimInstance Win32_Process | Where-Object {
    ($_.Name -in @('python.exe', 'pyocd.exe') -and
        $_.CommandLine -match 'pyocd.*gdbserver' -and
        $_.CommandLine -match '--port\s+3333' -and
        $_.CommandLine -match 'MSPM0G3507') -or
    ($_.Name -eq 'powershell.exe' -and
        $_.CommandLine -match 'start-mspm0g3507-app-debug\.ps1')
})

foreach ($process in $remaining) {
    Stop-Process -Id $process.ProcessId -Force -ErrorAction SilentlyContinue
}

Write-Host "Stopped MSPM0G3507 app pyOCD server processes: $($remaining.Count)"
