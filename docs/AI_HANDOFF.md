# MSPM0L1306 AI Context Handoff

## Mandatory Workflow

Every AI must read this document in full before inspecting, modifying,
building, debugging, or programming this repository. At the end of every
development task, update this document before the handoff with the changes
made, verification actually run, hardware/tool discoveries, and unresolved
risks. `AGENTS.md` enforces this rule at the repository root.

Every Git commit must contain a concise subject and a detailed body covering
behavioral changes, affected tools or hardware, and verification evidence.

## Project State

- Target: MSPM0L1306, VQFN-32 (RHB), TI MSPM0 SDK 2.05.01.00.
- Application: PA3 LED output toggles every 500 ms; UART0 echoes received
  bytes through a 64-byte interrupt-safe FIFO.
- SysConfig source: `mspm0l1306_bringup.syscfg`.
- Firmware entry point: `app/main.c`.
- GPIO wrapper: `board/board_led.c`.
- UART wrapper: `board/board_uart.c`.

## Pin Assignment And Wiring

| Function | MCU pin | DAPLink connection |
| --- | --- | --- |
| LED output | PA3 | Board LED circuit |
| UART0 TX | PA8 | DAPLink RX |
| UART0 RX | PA9 | DAPLink TX |
| SWDIO | PA19 | DAPLink SWDIO |
| SWCLK | PA20 | DAPLink SWCLK |
| Ground | GND | DAPLink GND |

Use 3.3 V logic. Do not power the core board from multiple sources at once.
The DAPLink virtual UART is currently `COM37`; re-check it before relying on
that number.

## Toolchain

- CCS: `D:\Software\ti\ccs2020` (20.2.0.00012). Use for SysConfig graphical
  edits and optional project import only.
- MSPM0 SDK: `D:\Software\ti\mspm0_sdk_2_05_01_00` (2.05.01.00).
- TI Clang: `D:\Software\ti\ccs2020\ccs\tools\compiler\ti-cgt-armllvm_4.0.3.LTS\bin\tiarmclang.exe`.
- GNU Make: `D:\Software\ti\ccs2020\ccs\utils\bin\gmake.exe`.
- ARM GDB: `D:\Software\STM32CubeCLT_1.18.0\GNU-tools-for-STM32\bin\arm-none-eabi-gdb.exe`.
- pyOCD: local `.venv`, version 0.45.0.
- Probe: Horco CMSIS-DAP v2, UID `ad000b52`.

## Build, Debug, And Flash

Build only from the repository root with:

```powershell
& 'D:\Software\ti\ccs2020\ccs\utils\bin\gmake.exe' -C ticlang all
```

SysConfig generated files must remain in `ticlang` root. Do not move them to
`ticlang\syscfg`.

`tools/flash-and-debug.ps1` is the only standard pyOCD wrapper:

```powershell
powershell -ExecutionPolicy Bypass -File tools\flash-and-debug.ps1 -Action List
powershell -ExecutionPolicy Bypass -File tools\flash-and-debug.ps1 -Action GdbServer
powershell -ExecutionPolicy Bypass -File tools\flash-and-debug.ps1 -Action Load
```

`Load` can erase and write target Flash. Obtain explicit user awareness before
running it. `List`, `Build`, and `GdbServer` do not program Flash.

TI Clang produces an ELF32 file with a `.out` extension. pyOCD does not infer
ELF from `.out`, so the wrapper must retain `--format elf` for `Load`.

TI's CMSIS-Pack requires an AP0 ROM-table datapatch that pyOCD 0.45.0 does not
apply. `tools/pyocd-mspm0l1306.py` injects `0xF0000000` after AP creation; the
wrapper must retain `--script` for `GdbServer` and `Load`.

## VS Code

Use the tracked `.vscode` configuration:

- `MSPM0: Build` builds through TI gmake.
- `MSPM0: List pyOCD probes` is non-destructive.
- `MSPM0: Start pyOCD GDB server` starts the external debug server.
- `MSPM0: Load firmware to Flash (erases/writes target)` programs Flash.
- F5 configuration: `MSPM0L1306: Attach to pyOCD`.

## Verification History

- Host FIFO tests: `powershell -ExecutionPolicy Bypass -File tests\run_host_tests.ps1` passes 4 tests.
- LED pin configuration test: `powershell -ExecutionPolicy Bypass -File tests\test_led_pin_config.ps1` verifies PA3.
- pyOCD compatibility-script test: `.\.venv\Scripts\python.exe tests\test_pyocd_mspm0l1306_script.py` passes.
- Flash wrapper test: `powershell -ExecutionPolicy Bypass -File tests\test_flash_wrapper.ps1` verifies `--format elf`.
- Hardware verified by user: DAPLink programs successfully, PA3 toggles, and
  UART0 echo works through the connected virtual serial port.

## Repository Hygiene

Keep generated and local-only content out of Git: `.venv/`, `tools/packs/`,
`tools/*.log`, `ticlang/*.obj`, `ticlang/*.out`, generated SysConfig outputs,
and `tests/build/`. The existing `.gitignore` defines the complete list.

## Repository History

- Git repository initialized on `main`. The root commit documents the PA3 LED,
  UART echo, VS Code, pyOCD compatibility, and their validation. The commit
  message policy above was added and applied by amending that root commit.
