# MSPM0G3507 Keil MDK application

This is an independent Keil MDK project for the existing MSPM0G3507 FreeRTOS
application. The application source files are shared from `../../mspm0g3507_app`;
the CCS project remains the owner of the original `.project`, `.cproject`, and
SysConfig source files.

## Prerequisites

- Keil MDK 5.43a with Arm Compiler 6.24 at `D:\Keil_v5`.
- MSPM0 SDK 2.11.00.07 at `D:\Software\ti\ccs2020\mspm0_sdk_2_11_00_07`.
- SysConfig 1.26.2 at `D:\Software\ti\ccs2020\sysconfig_1.26.2`.
- Installed CMSIS-Pack `TexasInstruments.MSPM0G1X0X_G3X0X_DFP.1.3.1`.

Install `D:\desktop\2026_electronics_design_contest\tools\packs\TexasInstruments.MSPM0G1X0X_G3X0X_DFP.1.3.1.pack` with Keil Pack Installer. Select `MSPM0G3507` in the Texas Instruments device family and confirm that the project uses the `MSPM0G1X0X_G3X0X_MAIN_128KB.FLM` main Flash algorithm and `MSPM0G350X.svd`.

## Build

From the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-keil-mspm0g3507-app.ps1
```

The script generates SysConfig output with `--compiler keil`, invokes the Keil
UV4 batch build, and checks that protected CCS and VS Code files are unchanged.
It does not erase or program the target. The debug AXF is written to
`keil\mspm0g3507_app\Objects\mspm0g3507_app.axf` and HEX files are written to
`Objects\`; the linker MAP file is written to
`keil\mspm0g3507_app\mspm0g3507_app.map`.

The project uses the SDK GCC Cortex-M0 FreeRTOS port in the local
`freertos_port\ARM_CM0` directory. Arm Compiler 6 must use short enum and short
wchar ABI settings (`vShortEn=1`, `vShortWch=1`) to match the SDK Keil DriverLib
archive.

## Debug and download

The project is configured for CMSIS-DAP/SWD with `DriverSelection=4096`.
Connect the DAPLink probe as follows:

- SWDIO to PA19
- SWCLK to PA20
- VTref to 3.3 V
- GND common
- Probe UID: `2dd0719d`

Use Keil's target settings to select the CMSIS-DAP probe, then use Download or
Start/Stop Debugging explicitly. Hardware connection, Flash programming,
breakpoints, single stepping, and register inspection are not claimed by the
repository build checks unless they have been performed on the physical target.

The debugger can inspect `g_encoder_samples[0]` through
`g_encoder_samples[3]`. Do not place breakpoints while motors are moving.
