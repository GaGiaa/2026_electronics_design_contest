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

## G3507 FreeRTOS 工程

`mspm0g3507_freertos/` 是 G3507 裸机工程之外的独立 FreeRTOS 基线，必须与
`mspm0g3507_bringup/` 共存。它使用同一块天猛星核心板的 `PB22` 用户 LED 与
UART0 `PA10` TX、`PA11` RX，但不复用裸机工程的 SysConfig 生成文件或 ELF。

构建命令：

```powershell
.\tools\build-mspm0g3507-freertos.ps1
```

构建脚本从 SDK 2.11.00.07 的只读 FreeRTOS 内核和 TI ARM Clang Cortex-M0+
移植层编译，工程本地 `FreeRTOSConfig.h` 固定为 1 kHz 抢占式 tick，开启静态
对象分配和栈溢出检查，禁用动态对象分配及软件定时器。应用创建两个静态任务：
LED 任务通过 `vTaskDelayUntil()` 使用 `LED_TOGGLE_INTERVAL_MS` 翻转电平；
UART 回显任务阻塞接收静态队列。UART RX ISR 必须使用 `xQueueSendFromISR()`；
不得恢复裸机的 `SysTick_Handler` 或 `EchoQueue`。

RTOS ELF 为 `mspm0g3507_freertos/Debug/mspm0g3507_freertos.out`。VS Code
构建任务为 `MSPM0G3507 FreeRTOS: Build`，调试配置为
`MSPM0G3507 FreeRTOS: Attach to pyOCD`。启动 GDB server 仍使用原有 G3507
脚本；要写入 RTOS ELF 必须显式传入 `-Firmware freertos`：

```powershell
powershell -ExecutionPolicy Bypass -File tools\flash-and-debug-g3507.ps1 -Action Load -Firmware freertos
```

该命令会 `--erase chip`，执行前必须取得用户明确同意。已验证 SysConfig 生成、
TI Clang 编译、FreeRTOS 内核链接和静态集成检查；尚未执行探针枚举、Flash 写入、
PB22 LED 实测或 UART 回显实测。

## G3507 正式应用工程

`mspm0g3507_app/` 是 G3507 的长期 FreeRTOS 应用工程，与 bring-up 和
`mspm0g3507_freertos/` 基线独立。UART0 保持 PA10 TX、PA11 RX、115200 8-N-1，
接收 FIFO 在中断中排空，并通过静态队列回显。PB22 经 3.3 V 至 5 V 电平转换器驱动
4 颗标准 800 kHz WS2812；MCU、转换器和 LED 5 V 电源必须共地。

构建命令：

```powershell
.\tools\build-mspm0g3507-app.ps1
powershell -ExecutionPolicy Bypass -File tests\test_mspm0g3507_app.ps1
```

应用 ELF 为 `mspm0g3507_app/Debug/mspm0g3507_app.out`。默认动画每 500 ms
只点亮一颗灯珠，按红、绿、蓝、白及灯珠 1 至 4 的顺序循环，通道亮度为 16。
WS2812 使用 SPI1 PICO 输出到 PB22，SPI 目标速率为 2.666667 MHz，编码为
`0=100`、`1=110`；PB9 是未连接的 SPI 时钟输出。每帧含 36 字节编码数据和 24 字节
低电平锁存数据。发送期间关闭中断，发送后 PB22 保持低电平至少 80 us，未占用
SysTick 或替换 FreeRTOS 时钟处理。烧录该 ELF 需要显式传入 `-Firmware app`，且
`Load` 会擦除目标 Flash，未经用户明确授权不得执行。

已完成静态集成检查和 TI Clang 构建验证；尚未执行探针枚举、GDB 服务、Flash 写入、
LED 实测或 UART 连续回显实测。

应用工程已配置 40 MHz HFXT 经 SYSPLL 的 80 MHz CPU 时钟，FreeRTOS 的
`configCPU_CLOCK_HZ` 同步为 80 MHz；ULPCLK 经 UDIV /2 保持在 40 MHz 以下。
底盘电机使用双 PWM 输入；按当前底盘校准后的逻辑轮位为：前左 PA29/PB27、前右
PB4/PB5、后左 PA28/PA31、后右 PA12/PA13。四组定时器均为 40 MHz 时钟与 4000
计数周期，输出 10 kHz。
`main.c` 中每轮都有方向和占空比宏，默认占空比为 `0%`，因此安全停转；静态电机任务每
10 ms 应用命令。正转为 IN1 PWM/IN2 低，反转为 IN1 低/IN2 PWM，停止时两路均低。
已通过静态集成检查、SysConfig 生成和 TI Clang 构建；未执行 Flash 写入或硬件电机测试。

### 电机轮位校准

`board_motor` 的逻辑轮位已按当前底盘接线校准：逻辑前左使用 PA29/PB27，逻辑前右使用
PB4/PB5，逻辑后左使用 PA28/PA31 且正反方向反相，逻辑后右使用 PA12/PA13。
因此，四个逻辑轮位的 `BOARD_MOTOR_DIRECTION_FORWARD` 都表示车辆前进方向。重新接线或
更换电机驱动板后，必须重新进行单轮校准。

### 四轮编码器采样

`mspm0g3507_app/board_encoder.*` 提供四轮 AB 相增量编码器的独立驱动，逻辑轮位的 A/B
输入依次为：前左 PA16/PB20、前右 PA14/PA9、后左 PA15/PB24、后右 PA17/PA22。八个输入
均为上拉，四个 A 相配置双沿 GPIO 中断，B 相仅在 A 相边沿时读取以判定方向；GPIO 中断不调用
FreeRTOS API。默认以参考电机的单沿 520 计数为依据，采用 A 相双沿的
`BOARD_ENCODER_COUNTS_PER_REVOLUTION=1040` 与 `BOARD_ENCODER_WHEEL_DIAMETER_MM=48`。

电机静态任务仍以 10 ms 周期运行，且每轮均先通过 `board_encoder_sample()` 原子取得并清零本周期
有符号计数，再按原有开环方向和占空比命令更新 PWM。采样结构包含 `delta_counts`、
`total_counts` 和 `speed_mm_per_s`；本次未引入 PI 闭环或改变默认 0% 停转。已于 2026-07-21
通过 `tests/test_mspm0g3507_app.ps1` 静态集成检查，并通过
`tools/build-mspm0g3507-app.ps1` 的 SysConfig 生成、TI Clang 编译和 ELF 链接。已执行 app ELF
烧录；用户已确认烧录后的 UART 遥测和串口回显恢复正常。编码器计数正负方向和完整一圈脉冲数仍须
逐轮低占空比确认，并据此校准 1040 常量。

### 编码器观测与遥测

应用的 `main.c` 导出 `volatile board_encoder_sample_t g_encoder_samples[BOARD_MOTOR_COUNT]`，
由 10 ms 电机任务在完成四轮采样后写入。SWD 调试时可在 Live Expressions/Watch 中观察该全局数组，
但禁止在电机运动中设置断点，以免暂停 PWM、FreeRTOS 时基和编码器边沿处理。推荐先在不驱动或低占空比
条件下手动逐轮转动，确认单轮隔离、正反向计数符号和一圈脉冲数。

低优先级遥测任务每 100 ms 经 UART0 输出一行 `enc` CSV 数据；每轮字段顺序是
`delta_counts,total_counts,speed_mm_per_s`，速度以截断后的整数 mm/s 表示。UART 回显和遥测均通过
`board_uart_write()` 入队，内部使用静态帧队列；专用 `board_uart_tx_task` 独占 UART FIFO，
确保完整报文不会互相交错。工程仍禁用动态内存分配，遥测不得移动到 GPIO ISR 或 10 ms 电机任务中。
`main.c` 的 `ENCODER_TELEMETRY_ENABLE` 默认是 `0U`；改为 `1U` 会在编译期启用
`telemetry_task`、其静态栈和 `enc` 报文，同时保留电机任务中的编码器采样、调试器快照、
IMU 输出、UART 回显和 UART TX 任务。
首次硬件测试时，GDB 确认 `telemetry` 任务触发了 `vApplicationStackOverflowHook()`；原因是包含多次
格式化调用的遥测任务仅有 256 words 栈。已将 `ENCODER_TELEMETRY_TASK_STACK_DEPTH` 增至 512U，
并由 `tests/test_mspm0g3507_app.ps1` 固定检查。重新构建、烧录后，用户已确认 100 ms `enc` 遥测和
UART 回显均正常。

### 当前工作树交接状态

本次工作位于 `develop_1` 分支，本次新增/修改内容如下：

- `mspm0g3507_app/board_encoder.c/.h`：四轮 GPIO 编码器驱动。参考工程只用于提取引脚和 520
  计数参数；新驱动未复用参考代码。A 相使用双沿 GPIOA 中断，B 相在边沿时读取判向；引脚为
  前左 PA16/PB20、前右 PA14/PA9、后左 PA15/PB24、后右 PA17/PA22。驱动使用 SysConfig
  生成的 `ENCODER_*` 宏，不要手工编辑 `Debug/ti_msp_dl_config.*`。
- `mspm0g3507_app/main.c`：`motor_task` 仍为 10 ms，循环顺序固定为四轮编码器采样、再四轮
  PWM 输出。`g_encoder_samples[BOARD_MOTOR_COUNT]` 是可通过 SWD Watch/Live Expressions 观察的
  `volatile` 全局数组；序列号用于遥测任务读取一致快照。遥测任务栈为 512 words，避免格式化
  编码器报文时触发 FreeRTOS 栈溢出。
- UART 观测：`telemetry_task` 优先级 0、周期 100 ms，输出格式为
  `enc,fl=delta,total,speed,fr=delta,total,speed,rl=delta,total,speed,rr=delta,total,speed`，
  速度为截断整数 mm/s。`board_uart_write()` 将整帧复制到 8 槽静态发送队列；
  `board_uart_tx_task` 优先级 1 独占发送 FIFO，回显任务和遥测任务不直接操作 FIFO，避免之前
  “遥测持锁轮询 FIFO 导致回显接收队列溢出”的问题。
- `mspm0g3507_app/FreeRTOSConfig.h`：动态内存仍为禁用状态；不需要 `configUSE_MUTEXES`。
- `tests/test_mspm0g3507_app.ps1`：已覆盖编码器文件、八个引脚、A 相双沿/B 相上拉、任务顺序、
  快照全局变量、遥测任务、静态 UART 帧队列和构建脚本接入。
- `tools/build-mspm0g3507-app.ps1`：已加入 `board_encoder.c`，SysConfig 生成文件仍由脚本生成。

最近验证证据：

```powershell
powershell -ExecutionPolicy Bypass -File tests\test_mspm0g3507_app.ps1
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-app.ps1
```

两条命令在当前 512-word telemetry 栈版本均返回成功。构建只产生/更新被忽略的
`mspm0g3507_app/Debug/` 产物。SysConfig 仍会输出既有的 Flash 状态位和 PWM/低功耗保持提示，
这些不是本次构建失败。随后已烧录该 app ELF，用户确认 UART 遥测和回显均恢复正常。

下一步优先级：

1. 保持四路占空比 0%，通过 SWD 手动转轮，观察 `g_encoder_samples` 的单轮隔离、正反
   符号和一圈脉冲数；确认后再设置一个轮子的低占空比。
2. 持续接收 UART0 PA10/PA11 的 100 ms `enc` 报文，确认回显与遥测不交错。若需连续主机输入，
   重点观察 `board_uart_rx_overflow_count()` 是否保持不变。
3. 以实测完整一圈计数校准 `BOARD_ENCODER_COUNTS_PER_REVOLUTION`；当前默认是 A 相双沿的 1040，
   不是硬件 QEI 四倍计数。

已知设计边界：MSPM0G3507 这块硬件只有一个可用定时器 QEI，当前八根参考编码器线也不能组成
四组同一 `TIMGx` 的 CCP0/CCP1，因此本版本明确采用 GPIO 中断。不要在没有重新分配硬件引脚和
确认定时器资源前，把四路 GPIO 驱动改成“四个 QEI”或盲目改成 Timer Capture；Timer Capture
本身不能自动完成 AB 相方向解码。
### PA2 无源蜂鸣器

`mspm0g3507_app/` 将 PA2 配置为 TIMG8 CCP1 硬件 PWM 输出。`main.c` 的
`BUZZER_FEATURE_ENABLE`、`BUZZER_FREQUENCY_HZ`、`BUZZER_DUTY_PERCENT`、
`BUZZER_ON_TIME_MS` 和 `BUZZER_OFF_TIME_MS` 为编译期配置宏；默认分别为
关闭、2000 Hz、50%、200 ms 和 1800 ms。启用时新增一个静态 FreeRTOS 任务循环
响/停，禁用时驱动初始化后保持 PA2 低电平且不创建任务。已验证 SysConfig 生成、
静态集成测试与 TI Clang 构建；尚未进行 Flash 写入或蜂鸣器硬件实测。蜂鸣器驱动
电路必须与 MCU 共地。

### 编码器通道校准更新

通过手动转轮硬件校准，已确定以下物理引脚到逻辑轮位的编码器映射，
并已直接写入 `mspm0g3507_app.syscfg`：

- 逻辑前左轮：PA15/PB24；
- 逻辑前右轮：PA17/PA22，方向取反；
- 逻辑后左轮：PA14/PA9；
- 逻辑后右轮：PA16/PB20，方向取反。

生成的 `ENCODER_*` 宏现在直接表示上述逻辑轮位；
`board_encoder_gpioa_irq_handler()` 只应用两路已校准的方向符号。PWM 电机映射保持不变。
在完成每个车轮已知圈数的实测前，`BOARD_ENCODER_COUNTS_PER_REVOLUTION` 仍保持为 1040。

## G3507 Keil MDK 迁移

`keil/mspm0g3507_app/` 是现有 `mspm0g3507_app/` 应用的独立 Keil MDK 工程。
Keil 工程通过相对路径共享应用的 C/H 文件和原始 SysConfig 源文件；不会修改 CCS
的 `.project`、`.cproject`、原始 `.syscfg`、原始构建脚本或 VS Code 调试配置。

该工程以 `MSPM0G3507` 为目标，使用 Arm Compiler 6、SDK 提供的 Keil 启动文件和
scatter 文件，以及本地 FreeRTOS `GCC/ARM_CM0` 移植层副本。本地移植层用于保持
Keil 构建边界与 TI Clang 移植层分离。由于 SDK Keil 版 `driverlib.a` 使用特定 ABI，
Keil 编译器必须启用短枚举和短 wchar（`vShortEn=1`、`vShortWch=1`），否则会出现
`wchart-16`/`wchart-32` 以及 packed-enum/enum-is-int ABI 冲突。

SysConfig 使用 `--compiler keil` 生成到 `keil/mspm0g3507_app/Generated/`；
原始 `mspm0g3507_app/Debug/` 不会被使用或修改。Keil 工程选择 CMSIS-DAP
（`DriverSelection=4096`）、`MSPM0G1X0X_G3X0X_MAIN_128KB.FLM` 算法和
`MSPM0G350X.svd`。构建脚本为 `tools/build-keil-mspm0g3507-app.ps1`，
只生成位于 `keil/mspm0g3507_app/Objects/` 的调试 AXF，不会编程 Flash。

静态检查和临时 AC6 编译/链接检查已覆盖 SysConfig Keil 输出、应用源码、FreeRTOS
内核、本地 M0 移植层、启动文件、scatter 文件和 DriverLib。CMSIS-Pack 压缩包包含
MSPM0G3507 器件、MSPM0G350X SVD，以及 128 KB、64 KB、32 KB 主 Flash 算法。

2026-07-21 的验证命令均已完成：

```powershell
powershell -ExecutionPolicy Bypass -File tests\test_mspm0g3507_app.ps1
powershell -ExecutionPolicy Bypass -File tests\test_keil_mspm0g3507_app.ps1
powershell -ExecutionPolicy Bypass -File tools\build-keil-mspm0g3507-app.ps1
```

UV4 报告 `0 Error(s), 0 Warning(s)`，并生成调试 AXF/HEX；MAP 文件生成在 Keil 工程
目录旁。`fromelf` 已确认调试段、`motor_task` 函数符号以及带类型信息的
`g_encoder_samples[4]` 符号。Keil Pack 已安装到本地 MDK Pack 根目录，
`cpackget list` 报告版本为 1.3.1。

截至本次交接，实体 CMSIS-DAP 枚举、Flash 下载、Keil 断点/单步、寄存器查看以及
`g_encoder_samples` 实时观察仍属于硬件验收事项；未实际连接探针时不得报告为成功。
### BMI160 六轴 IMU 接入

BMI160 首版使用 G3507 的独立硬件 SPI0，不使用原接线图中的 I2C 方案。
MSPM0G3507 的 PB17/PB18 没有 I2C 复用，但支持 SPI0：模块 `SCK` 接 PB18，
`SDI`/MOSI 接 PB17，`SDO`/MISO 接 PB19，低有效 `CS` 接 PB0。PA21 保留给
后续数据就绪中断，首版不启用；SPI 模式下 SA0 不参与地址选择。SPI1 仍由
WS2812 使用。

模块必须使用 3.3 V 并与 MCU 共地；不要在未核对模块原理图前同时给 VIN 和
3V3 供电。若模块只有 SDA/SCL 标注，必须确认其是否支持 SPI 并找出独立的
SDI、SDO 和 SCK 引脚，不能把 I2C SDA 直接当作完整 SPI 接线。

`mspm0g3507_app.syscfg` 中的 `SPI_BMI160` 配置为 SPI0、8 MHz、8 位、MSB
first、Motorola mode 3，PB0 配置为高电平空闲 GPIO CS。上电后驱动先拉低再
拉高 CS 并发送一次 `0xFF` dummy SPI 事务，以完成 BMI160 的 SPI 接口选择；该
启动步骤与 Bosch COINES 和公开 SPI 移植例程一致。该模式与 Bosch BMI160_SensorAPI
官方 `read_sensor_data` 例程一致。`board_bmi160.c/.h` 提供
`board_bmi160_init(uint8_t *)` 和 `board_bmi160_read_sample(...)`，使用状态码、
有界 SPI 超时和 CS 错误释放。初始化读取 `CHIP_ID` 并要求 `0xD1`，执行软复位，
配置加速度计 ±4g/100 Hz、陀螺仪 ±500dps/100 Hz；软复位后按 Bosch 官方流程读取
0x7F 重新启用 SPI，并在每次寄存器写入后等待 1 ms，再从陀螺仪数据起始寄存器
0x0C 连续读取至加速度数据结束寄存器 0x17 的 12 字节，并按“陀螺仪三轴在前、
加速度三轴在后”解析六个有符号原始值。

独立静态 FreeRTOS IMU 任务每 10 ms 读取一次数据，并复用目标基线已有的
`board_uart_write()` 静态帧队列输出：

```text
bmi160,id=0xD1,status=0
imu,ax=-123,ay=456,az=8192,gx=2,gy=-1,gz=0
```

初始化失败每秒重试，连续三次采样失败后重新初始化。当前已完成目标提交
`2f644e99dd4606ab2ba911fb29d4e2e813da77c9` 基线迁移、静态集成检查、SysConfig
生成和 TI Clang 构建。硬件已验证 `CHIP_ID=0xD1`、静止 Z 轴约 1g、陀螺仪三轴
接近 0dps，编码器遥测、IMU 报文和 UART 回显未出现交叉损坏。后续烧录仍需
遵循明确授权原则。

`main.c` 中的 `IMU_TELEMETRY_ENABLE` 是 IMU 串口输出的编译期开关，默认值为
`0U`。设为 `1U` 后输出初始化状态、采样错误和六轴原始数据；设为 `0U` 时仅
关闭这些 UART 报文，IMU 任务仍会初始化 BMI160、周期采样并在连续失败后重试。

### 四个低有效按键

`mspm0g3507_app.syscfg` 新增 `BUTTONS` GPIO 组，按键输入依次为 PA7、PB12、PA8
和 PA30。四个引脚均为普通输入，不启用 GPIO 中断，也不配置内部上下拉；硬件必须
提供外部上拉，按下时将输入拉至低电平。

`board_buttons.c/.h` 提供独立的按键状态驱动。`board_buttons_init()` 只记录上电时
的当前状态，`board_buttons_scan()` 每次扫描要求连续两个相同采样才确认变化。新增的
静态 `button_task` 每 10 ms 运行一次，按 PA7、PB12、PA8、PA30 顺序经
`board_uart_write()` 上报稳定边沿：

```text
key,pa7=down\r\n
key,pa7=up\r\n
```

按键任务使用 `BUTTON_TASK_STACK_DEPTH=128U`，不新增队列、不使用中断，也不合并现有
WS2812、蜂鸣器、IMU、UART 或编码器任务。TI Clang map 中该任务栈为 512 bytes，
任务控制块为 76 bytes，按键驱动运行态数据为 12 bytes，按键驱动代码约 568 bytes。
`main.c` 中的 `BUTTON_FEATURE_ENABLE` 默认为 `0U`；设为 `1U` 后启用按键初始化、
扫描、按键报文和按键任务的静态资源。无论开关取值如何，SysConfig 引脚定义均保留。

已通过以下验证：

```powershell
powershell -ExecutionPolicy Bypass -File tests\test_mspm0g3507_app.ps1
powershell -ExecutionPolicy Bypass -File tests\test_keil_mspm0g3507_app.ps1
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-app.ps1
powershell -ExecutionPolicy Bypass -File tools\build-keil-mspm0g3507-app.ps1
```

TI Clang 和 Keil 构建均已生成目标文件；Keil 工程同时补齐了此前 BMI160 分支遗漏的
`board_bmi160.c` 源文件。当前尚未进行四个按键的实体按压、释放和抗抖硬件验收，也未
执行 Flash 擦除或烧录。
### 八路灰度传感器接入

`mspm0g3507_app/board_grayscale.c/.h` 已接入感为无 MCU 八路灰度传感器。
传感器使用 74HC4051 复用模拟输出，物理接线固定为 AD0 -> PB13、AD1 -> PB1、
AD2 -> PB23、OUT -> PA27；EN 悬空，利用模块内部下拉保持低电平使能。传感器
必须与 MCU 共地，并使用稳定的独立 5 V 供电。软件通道顺序为地址升序：通道 0
对应 AD2:AD1:AD0=000，通道 7 对应 111。

`mspm0g3507_app.syscfg` 将 PA27 配置为 ADC0 单通道、12 位、VDDA 参考，
PB13/PB1/PB23 配置为 AD0/AD1/AD2 推挽输出。驱动每次切换地址后等待约 1 us，
对每路执行 8 次单次 ADC 转换并取平均，避免重复转换模式下同步等待无法结束。
原始值、0..4095 归一化值和带滞回的 8 位数字值发布到 `g_grayscale_snapshot`。

应用灰度任务使用静态内存，每 10 ms 采样一次；`GRAY_TELEMETRY_ENABLE` 默认
为 `0U`，设为 `1U` 后每 100 ms 通过现有串行帧队列输出 `gray,raw=...,norm=...,
digital=0x..`。当前默认白值为每路 3000、黑值为每路 500，仅是起始标定参数，
必须在固定实际安装高度后替换为实测值。

本次已完成灰度静态集成检查、CCS SysConfig/TI Clang 构建和 Keil SysConfig/UV4
构建。Keil 构建日志为 `0 Error(s), 0 Warning(s)`，并生成 AXF/HEX。尚未完成
灰度传感器实物接线、烧录、逐路地址响应、白黑读数和 UART 实测验收；不能仅凭
编译结果宣称硬件验收完成。

后续修复：Keil 某些 SysConfig 生成结果只生成 ADC memory 配置，没有生成
`DL_ADC12_initSingleSample()`，导致灰度任务在 `DL_ADC12_isConversionStarted()`
等待处停住，`g_grayscale_snapshot.sequence` 保持 0。`board_grayscale_init()`
现在显式关闭并初始化 ADC 为单次、自动采样、软件触发、12 位无符号模式后再
重新使能，避免依赖生成器是否输出该控制模式初始化。修复已通过 CCS/Keil
构建；仍需用最新 AXF/HEX 重新下载后进行硬件确认。
