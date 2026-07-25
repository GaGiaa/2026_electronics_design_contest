# MSPM0G3507 FreeRTOS 基线工程

本工程是独立的 MSPM0G3507 CCS FreeRTOS 基线，用于验证静态任务、静态队列和 UART
中断接收流程。项目总览见根目录 [`README.md`](../README.md)，工具版本和源码依赖见
[`docs/DEPENDENCIES.md`](../docs/DEPENDENCIES.md)，跨电脑构建步骤见
[`docs/SETUP.md`](../docs/SETUP.md)。

## 功能与资源

- 用户 LED 使用 `PB22`；
- UART0 使用 `PA10`（TX）和 `PA11`（RX）；
- UART 配置为 115200、8-N-1，中断接收并回显数据；
- FreeRTOS tick 为 1 kHz；
- 工程包含两个静态分配任务和一个静态分配 UART 接收队列；
- `configSUPPORT_STATIC_ALLOCATION=1`，`configSUPPORT_DYNAMIC_ALLOCATION=0`；
- 不使用 FreeRTOS 动态对象分配。

## CCS 与构建

在仓库外创建 CCS workspace，通过 `Project -> Import CCS Projects` 导入本目录。可使用
以下脚本执行可复现构建：

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-freertos.ps1
```

输出文件为 `mspm0g3507_freertos/Debug/mspm0g3507_freertos.out`。构建、SysConfig 生成
和静态检查不会擦除或写入 Flash；任何烧录或目标板调试操作都必须取得用户明确授权。

本工程的详细 C 注释和 FreeRTOS 任务说明规范见
[`docs/CODING_STYLE.md`](../docs/CODING_STYLE.md)。
