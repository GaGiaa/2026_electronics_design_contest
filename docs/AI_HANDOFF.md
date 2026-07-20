# MSPM0 核心板工程交接索引

## 使用规则

任何 AI 在查看、修改、构建、调试或烧录本仓库前，必须完整阅读本文件。每次任务完成前更新本文档，记录实际修改、验证结果、硬件或工具事实与遗留风险。

提交 Git 时必须使用简洁中文标题和详细中文正文，正文应说明功能变化、涉及的工具或硬件，以及实际验证证据。

仓库同时维护 MSPM0L1306 和 MSPM0G3507 两个独立 CCS 工程。不得将两者的 SysConfig 生成文件、ELF、CMSIS-Pack、pyOCD 参数或引脚配置混用。

## 工程索引

| 核心板 | CCS 工程 | 芯片与封装 | LED | UART0 | Debug ELF |
| --- | --- | --- | --- | --- | --- |
| L1306 | `mspm0l1306_bringup/` | MSPM0L1306, VQFN-32 (RHB) | PA3 | PA8 TX, PA9 RX | `mspm0l1306_bringup/Debug/mspm0l1306_bringup.out` |
| G3507 | `mspm0g3507_bringup/` | MSPM0G3507, LQFP-64 (PM) | PB22 | PA10 TX, PA11 RX | `mspm0g3507_bringup/Debug/mspm0g3507_bringup.out` |

两套固件均使用 `LED_TOGGLE_INTERVAL_MS` 控制电平翻转间隔，默认值为 2000 ms。G3507 的 UART0 连到板载 CH340；使用前在主机上确认实际串口号。两块板均使用 PA19 作为 SWDIO、PA20 作为 SWCLK，并使用 3.3 V 逻辑电平。

## 工具链

- CCS: `D:\Software\ti\ccs2020`，用于 CCS 工程导入和 SysConfig 图形编辑。
- MSPM0 SDK: `D:\Software\ti\ccs2020\mspm0_sdk_2_11_00_07`。
- SysConfig: `D:\Software\ti\ccs2020\sysconfig_1.26.2`。
- TI Clang: `D:\Software\ti\ccs2020\ccs\tools\compiler\ti-cgt-armllvm_4.0.3.LTS\bin\tiarmclang.exe`。
- ARM GDB: `D:\Software\STM32CubeCLT_1.18.0\GNU-tools-for-STM32\bin\arm-none-eabi-gdb.exe`。
- pyOCD: 安装到 `tools/.venv`，版本约束为 `>=0.45,<0.46`。
- 调试器: Horco CMSIS-DAP v2。CCS 20.2 不会将其识别为 XDS 调试器；烧录与 SWD 调试使用 pyOCD。

## L1306 操作

构建:

```powershell
& 'D:\Software\ti\ccs2020\ccs\utils\bin\gmake.exe' -C mspm0l1306_bringup\Debug all
```

首次安装调试工具:

```powershell
powershell -ExecutionPolicy Bypass -File tools\install-debug-tools.ps1
```

pyOCD 操作使用 `tools/flash-and-debug.ps1`，其 `List`、`GdbServer` 和 `Load` 分别对应枚举、启动服务和擦除写入。L1306 必须使用 `tools/pyocd-mspm0l1306.py`，它在 CoreSight 发现前设置 AP0 ROM-table 地址；`GdbServer` 与 `Load` 必须保留该脚本。TI Clang 的 `.out` 为 ELF，`Load` 必须显式传递 `--format elf`。

VS Code 任务和调试配置分别是 `MSPM0: Build` 与 `MSPM0L1306: Attach to pyOCD`。

## G3507 操作

构建:

```powershell
.\tools\build-mspm0g3507.ps1
```

该脚本生成 G3507 专用的 `Debug/ti_msp_dl_config.*`，编译固件并链接 G3507 ELF。不要手工编辑 `Debug/` 中的生成文件。

首次安装调试工具:

```powershell
powershell -ExecutionPolicy Bypass -File tools\install-g3507-debug-tools.ps1
```

G3507 使用 `TexasInstruments.MSPM0G1X0X_G3X0X_DFP.1.3.1.pack` 和 `tools/flash-and-debug-g3507.ps1`。该 Pack 自带 G 系列的调试元数据，禁止传入 L1306 专用 `pyocd-mspm0l1306.py`。VS Code 任务以 `MSPM0G3507:` 开头，F5 配置为 `MSPM0G3507: Attach to pyOCD`。

## 烧录安全边界

两个烧录脚本的 `Load` 都会执行 `--erase chip` 并写入目标 Flash。执行前必须明确告知并获得用户同意。构建、`List` 和 `GdbServer` 不会写入 Flash；F5 只连接已启动的 `localhost:3333` GDB server。

## 已验证状态

- L1306: 已验证 CCS Debug 构建、L1306 SysConfig 与 pyOCD 脚本配置；未执行探针枚举、GDB 服务、硬件串口测试或 Flash 写入。
- G3507: 已验证 PB22、UART0 PA10/PA11 的 SysConfig 生成；`tools/build-mspm0g3507.ps1` 已完成 TI Clang 编译和 ELF 链接；G 系列 CMSIS-Pack 已安装且 pyOCD 已列出 `mspm0g3507` 目标。
- G3507: 未执行探针枚举、GDB 服务、Flash 写入、LED 实测或串口回显测试。

## Git 与文件卫生

不得提交 `mspm0l1306_bringup/Debug/`、`mspm0g3507_bringup/Debug/`、`tools/.venv/`、`tools/packs/`、Python 缓存、VS Code 本地状态或操作系统临时文件。完整规则以 `.gitignore` 为准。
