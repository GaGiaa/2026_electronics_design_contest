$ErrorActionPreference = 'Stop'

$syscfg = Get-Content -Raw (Join-Path $PSScriptRoot '..\mspm0l1306_bringup.syscfg')
if ($syscfg -notmatch 'led4\.associatedPins\[0\]\.pin\.\$assign = "PA3"') {
    throw 'The LED GPIO assignment must be PA3.'
}
