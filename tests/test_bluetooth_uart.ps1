[CmdletBinding()]
param(
    [string] $ProjectRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

function Assert-Path {
    param([Parameter(Mandatory)] [string] $Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing required Bluetooth host-link path: $Path"
    }
}

function Assert-Contains {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Pattern,
        [Parameter(Mandatory)] [string] $Description
    )
    if (-not (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) {
        throw "$Description - '$Pattern' was not found in $Path"
    }
}

function Assert-NotContains {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Pattern,
        [Parameter(Mandatory)] [string] $Description
    )
    if (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet) {
        throw "$Description - '$Pattern' was unexpectedly found in $Path"
    }
}

$projectDir = Join-Path $ProjectRoot 'mspm0g3507_app'
$syscfg = Join-Path $projectDir 'mspm0g3507_app.syscfg'
$driver = Join-Path $projectDir 'drivers/bluetooth_uart/board_bluetooth_uart.c'
$driverHeader = Join-Path $projectDir 'drivers/bluetooth_uart/board_bluetooth_uart.h'
$hostLink = Join-Path $projectDir 'protocols/host_link/host_link.c'
$hostLinkHeader = Join-Path $projectDir 'protocols/host_link/host_link.h'
$tasks = Join-Path $projectDir 'app/app_tasks_io.c'
$interrupts = Join-Path $projectDir 'platform/g3507_interrupts.c'
$interruptHeader = Join-Path $projectDir 'platform/g3507_interrupts.h'
$buildScript = Join-Path $ProjectRoot 'tools/build-mspm0g3507-app.ps1'
$keilBuildScript = Join-Path $ProjectRoot 'tools/build-keil-mspm0g3507-app.ps1'
$keilProject = Join-Path $ProjectRoot 'keil/mspm0g3507_app/mspm0g3507_app.uvprojx'
$ccsProject = Join-Path $projectDir '.cproject'
$layers = Join-Path $ProjectRoot 'tests/test_mspm0g3507_layers.ps1'
$appConfig = Join-Path $projectDir 'config/app_config.h'

foreach ($path in @($syscfg, $driver, $driverHeader, $hostLink, $hostLinkHeader, $tasks, $interrupts, $interruptHeader, $buildScript, $keilBuildScript, $keilProject, $ccsProject, $appConfig)) {
    Assert-Path -Path $path
}

Assert-Contains -Path $syscfg -Pattern 'UARTBluetooth\.\$name\s*=\s*"UART_2"' -Description 'SysConfig UART2 instance'
Assert-Contains -Path $syscfg -Pattern 'UARTBluetooth\.targetBaudRate\s*=\s*115200' -Description 'Bluetooth baud rate'
Assert-Contains -Path $syscfg -Pattern 'UARTBluetooth\.peripheral\.\$assign\s*=\s*"UART2"' -Description 'Bluetooth UART peripheral'
Assert-Contains -Path $syscfg -Pattern 'UARTBluetooth\.peripheral\.rxPin\.\$assign\s*=\s*"PB16"' -Description 'Bluetooth RX pin'
Assert-Contains -Path $syscfg -Pattern 'UARTBluetooth\.peripheral\.txPin\.\$assign\s*=\s*"PB15"' -Description 'Bluetooth TX pin'
Assert-Contains -Path $driver -Pattern 'UART_2_INST' -Description 'Bluetooth driver must use UART2'
Assert-Contains -Path $driver -Pattern 'xQueueSendFromISR' -Description 'Bluetooth RX ISR queue handoff'
Assert-Contains -Path $driver -Pattern 'xQueueCreateStatic' -Description 'Bluetooth TX queue static allocation'
Assert-Contains -Path $driverHeader -Pattern 'board_bluetooth_uart_disable' -Description 'Bluetooth driver disable API'
Assert-Contains -Path $driver -Pattern 'DL_UART_Main_disableInterrupt' -Description 'Bluetooth RX interrupt disable'
Assert-Contains -Path $driver -Pattern 'NVIC_DisableIRQ' -Description 'Bluetooth NVIC disable'
Assert-Contains -Path $driver -Pattern 'DL_UART_Main_disable' -Description 'Bluetooth UART peripheral disable'
Assert-Contains -Path $tasks -Pattern 'host_link_receive_byte' -Description 'Bluetooth task host-link dispatch'
Assert-Contains -Path $tasks -Pattern 'xQueueCreateStatic' -Description 'Bluetooth RX queue static allocation'
Assert-Contains -Path $appConfig -Pattern '#define APP_BLUETOOTH_UART_ENABLE 0U' -Description 'Bluetooth feature default off'
Assert-Contains -Path $appConfig -Pattern 'APP_BLUETOOTH_UART_ENABLE > 1U' -Description 'Bluetooth feature switch validation'
Assert-Contains -Path $tasks -Pattern '#if APP_BLUETOOTH_UART_ENABLE' -Description 'Bluetooth task compile-time gate'
Assert-Contains -Path $tasks -Pattern 'board_bluetooth_uart_disable' -Description 'Bluetooth disabled startup path'
Assert-Contains -Path $interrupts -Pattern 'UART_2_INST_IRQHandler' -Description 'Bluetooth UART2 interrupt dispatch'
Assert-Contains -Path $interruptHeader -Pattern 'UART_2_INST_IRQHandler' -Description 'Bluetooth UART2 interrupt declaration'
Assert-Contains -Path $interrupts -Pattern '#if APP_BLUETOOTH_UART_ENABLE' -Description 'Bluetooth interrupt compile-time gate'
Assert-Contains -Path $interrupts -Pattern 'UART_0_INST_IRQHandler' -Description 'UART0 interrupt must remain present'
Assert-Contains -Path $interrupts -Pattern 'UART_3_INST_IRQHandler' -Description 'UART3 interrupt must remain present'
Assert-Contains -Path $interrupts -Pattern 'board_crsf_uart_irq_handler' -Description 'CRSF interrupt must remain independent'
Assert-Contains -Path $buildScript -Pattern 'drivers\\bluetooth_uart\\board_bluetooth_uart\.c' -Description 'TI build Bluetooth driver source'
Assert-Contains -Path $buildScript -Pattern 'protocols\\host_link\\host_link\.c' -Description 'TI build host-link source'
Assert-Contains -Path $buildScript -Pattern 'BluetoothUartEnable' -Description 'TI build Bluetooth switch parameter'
Assert-Contains -Path $buildScript -Pattern 'APP_BLUETOOTH_UART_ENABLE=' -Description 'TI build Bluetooth compiler define'
Assert-Contains -Path $keilBuildScript -Pattern 'BluetoothUartEnable' -Description 'Keil build Bluetooth switch parameter'
Assert-Contains -Path $keilBuildScript -Pattern 'APP_BLUETOOTH_UART_ENABLE=' -Description 'Keil build Bluetooth compiler define'
Assert-Contains -Path $keilProject -Pattern 'drivers\\bluetooth_uart\\board_bluetooth_uart\.c' -Description 'Keil Bluetooth driver source'
Assert-Contains -Path $keilProject -Pattern 'protocols\\host_link\\host_link\.c' -Description 'Keil host-link source'
Assert-NotContains -Path $keilProject -Pattern 'APP_BLUETOOTH_UART_ENABLE=' -Description 'Keil project must defer Bluetooth feature default to app_config.h'
Assert-Contains -Path $layers -Pattern 'drivers\\bluetooth_uart' -Description 'Layer test Bluetooth driver directory'
Assert-Contains -Path $layers -Pattern 'protocols\\host_link' -Description 'Layer test host-link directory'
Assert-Contains -Path $ccsProject -Pattern '\$\{PROJECT_ROOT\}/drivers' -Description 'CCS recursive drivers source root'
Assert-Contains -Path $ccsProject -Pattern '\$\{PROJECT_ROOT\}/protocols' -Description 'CCS recursive protocols source root'

Write-Output 'MSPM0G3507 Bluetooth UART static integration checks passed.'
