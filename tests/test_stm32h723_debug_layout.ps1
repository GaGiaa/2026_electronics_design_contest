[CmdletBinding()]
param([string] $ProjectRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) { $ProjectRoot = Split-Path -Parent $PSScriptRoot }

$headerPath = Join-Path $ProjectRoot 'stm32h723_app\App\Inc\app_debug.h'
$header = Get-Content -Raw -LiteralPath $headerPath
foreach ($typeName in @(
    'h723_debug_system_t', 'h723_debug_uart8_t', 'h723_debug_crsf_t',
    'h723_debug_chassis_t', 'h723_debug_fdcan_t', 'h723_debug_jy901s_t'
)) {
    if ($header -notmatch "typedef struct \{[\s\S]*?\} $typeName;") {
        throw "Missing debug subgroup type: $typeName"
    }
}

foreach ($member in @(
    'h723_debug_system_t system;', 'h723_debug_uart8_t uart8;',
    'h723_debug_crsf_t crsf;', 'h723_debug_chassis_t chassis;',
    'h723_debug_fdcan_t fdcan;', 'h723_m2006_debug_t m2006[3];',
    'h723_debug_jy901s_t jy901s;'
)) {
    if ($header -notmatch [regex]::Escape($member)) { throw "Missing grouped debug member: $member" }
}

$sourceFiles = Get-ChildItem (Join-Path $ProjectRoot 'stm32h723_app\App\Src') -Filter '*.c'
$source = ($sourceFiles | ForEach-Object { Get-Content -Raw -LiteralPath $_.FullName }) -join "`n"
foreach ($path in @(
    'g_h723_debug.system.uptime_ms', 'g_h723_debug.uart8.tx_start_count',
    'g_h723_debug.crsf.channels_raw', 'g_h723_debug.chassis.left_target_rpm',
    'g_h723_debug.fdcan.rx_count', 'g_h723_debug.jy901s.angle_deg'
)) {
    if ($source -notmatch [regex]::Escape($path)) { throw "Missing grouped debug access: $path" }
}

if ($source -match 'g_h723_debug\.(boot_count|crsf_channels_raw|fdcan_rx_count|jy901s_angle_deg)\b') {
    throw 'Legacy flat debug member access remains in application source.'
}

Write-Output 'STM32H723 grouped debug layout test passed.'
