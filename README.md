# MSPM0L1306 Bring-up

This is a CCS 20.2 project for an MSPM0L1306 core board. It has two initial
functions:

- LED output on PA3 toggles every 500 ms.
- UART0 on PA8 (TX) and PA9 (RX) echoes received bytes at 115200 8-N-1.

## Tools

- CCS: `D:\Software\ti\ccs2020`
- MSPM0 SDK: `D:\Software\ti\mspm0_sdk_2_05_01_00`
- Probe: Horco CMSIS-DAP v2
- PC serial port: COM37

CCS is used for project import, SysConfig, and builds. The Horco probe is a
generic CMSIS-DAP v2 probe, which CCS 20.2 does not directly detect as an XDS
debug probe. Use pyOCD for flashing and SWD debug.

## Wiring

| DAPLink signal | Core board signal |
| --- | --- |
| GND | GND |
| SWDIO | PA19 / SWDIO |
| SWCLK | PA20 / SWCLK |
| TX | PA9 / UART0_RX |
| RX | PA8 / UART0_TX |

Use 3.3 V logic. Power the core board from only one source.

## Debug Tools

Run once from PowerShell:

```powershell
.\tools\install-debug-tools.ps1
```

The script creates `.venv`, installs pyOCD, and downloads TI's
MSPM0L11XX_L13XX CMSIS-Pack into `tools\packs`. The pack contains MSPM0L1306
Flash algorithms.

## CCS Workflow

1. Start CCS and select a workspace outside this repository.
2. Choose `Project -> Import CCS Projects` and select this folder.
3. Open `mspm0l1306_bringup.syscfg` to inspect or change the graphical setup.
4. Build the Debug configuration.
5. Run `tools\flash-and-debug.ps1 -Action Load` to program the ELF.

## VS Code Workflow

Open this repository folder in VS Code. CCS remains the tool for SysConfig
graphical edits and project import; VS Code is configured for everyday editing,
building, and SWD debugging.

1. Run the `MSPM0: Build` task to generate SysConfig files and build
   `ticlang\mspm0l1306_bringup.out`.
2. Run `MSPM0: List pyOCD probes` to confirm that the Horco CMSIS-DAP is
   visible.
3. Run `MSPM0: Start pyOCD GDB server`; keep its task terminal running.
4. Start `MSPM0L1306: Attach to pyOCD` with F5. It builds first, then attaches
   GDB to pyOCD at `localhost:3333`.

`MSPM0: Load firmware to Flash (erases/writes target)` is deliberately a
separate task. It erases and writes target Flash. The build, probe-list, and
GDB-server tasks do not program Flash.

## Serial Test

Open COM37 with 115200 baud, 8 data bits, no parity, one stop bit, and no flow
control. Send `abc123`; the terminal must receive exactly `abc123`.
