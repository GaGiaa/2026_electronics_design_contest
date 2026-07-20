# MSPM0G3507 Bring-up

CCS 20.2 原生工程，目标为天猛星 MSPM0G3507 LQFP-64 核心板。

## 功能与引脚

- `PB22` 用户 LED 按 `LED_TOGGLE_INTERVAL_MS` 翻转，默认间隔为 2000 ms。
- UART0 使用 `PA10`（TX）和 `PA11`（RX），通过板载 CH340 以 115200、8-N-1 回显接收数据。
- SWD 使用 `PA19`（SWDIO）和 `PA20`（SWCLK）。

## CCS 工作流

1. 在仓库外选择 CCS workspace。
2. 通过 `Project -> Import CCS Projects` 导入本目录。
3. 使用 `mspm0g3507_bringup.syscfg` 修改 G3507 的外设和引脚。
4. 构建 Debug 配置；输出为 `Debug\\mspm0g3507_bringup.out`。

不要复用 L1306 的 pyOCD Flash 脚本或 CMSIS-Pack。G3507 需要独立的
`TexasInstruments.MSPM0G1X0X_G3X0X_DFP` 包与目标配置；在确认目标被 pyOCD
识别前，不得执行 Flash 写入。
