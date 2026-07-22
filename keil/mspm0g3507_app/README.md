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

## Open from VS Code

Run the VS Code task `MSPM0G3507 App: Open Keil Project` to launch
`D:\Keil_v5\UV4\UV4.exe` with
`keil\mspm0g3507_app\mspm0g3507_app.uvprojx`. The task only opens the project;
it does not generate SysConfig files, build, download, or program Flash.

The launcher is `tools\open-keil-mspm0g3507-app.ps1`. If Keil is installed in
another directory, pass `-KeilRoot` when running the script or update its
default path.

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

The shared application also exposes `g_grayscale_snapshot` for SWD observation.
The grayscale sensor wiring is AD0=PB13, AD1=PB1, AD2=PB23, OUT=PA27, with EN
left floating and the sensor powered from a stable separate 5 V supply. Keil
generates the ADC and GPIO configuration from the CCS-owned SysConfig source;
do not edit `Generated/` manually. Physical sensor acceptance is separate from
the Keil build and requires checking all eight address selections.
The driver exposes `g_grayscale_adc_timeout_count` for SWD diagnostics. After
downloading a new AXF, a changing `g_grayscale_snapshot.sequence` confirms
sampling progress; a changing timeout counter means ADC0 is not reporting the
MEM0 result-complete flag.

The optional `board_imu_yaw.c` estimator is compiled into the shared
application project and runs within the existing IMU task when
`IMU_YAW_ENABLE` is changed from its default `0U` to `1U`. It uses the BMI160 Z
gyro, startup gyro-bias calibration, and differential left/right encoder speed
with the `IMU_YAW_TRACK_WIDTH_MM` value from `main.c`. The measured left/right
wheel-center distance is 130 mm and the vehicle frame is +X forward, +Y left,
+Z up. Enable `IMU_TELEMETRY_ENABLE` as well to observe the yaw fields
over UART. The output is a 16-byte JustFloat frame with channels
`yaw_deg`, `yaw_rate_dps`, and `gyro_bias_z_dps`. For the UART yaw test, use the Keil build overrides
`-VofaSpeedPidTelemetryEnable 0 -ImuTelemetryEnable 1 -ImuYawEnable 1`.
