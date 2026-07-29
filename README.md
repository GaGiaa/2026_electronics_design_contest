# 2026 电子设计竞赛工程

本仓库包含 MSPM0L1306、MSPM0G3507 的 bring-up 工程、FreeRTOS 基线工程、G3507
应用工程和 Keil MDK 工程。根目录 README 只负责项目导航和 AI 接手流程；工程细节、
依赖版本和实时验证状态分别维护在对应文档中。

## 工程地图

| 工程 | 用途 | 入口文档 |
| --- | --- | --- |
| `mspm0l1306_bringup/` | MSPM0L1306 核心板 LED、UART0 和 SWD 初始调试 | [`README.md`](mspm0l1306_bringup/README.md) |
| `mspm0g3507_bringup/` | MSPM0G3507 核心板 LED、UART0 和 SWD 初始调试 | [`README.md`](mspm0g3507_bringup/README.md) |
| `mspm0g3507_freertos/` | 独立的 G3507 FreeRTOS 静态分配基线 | [`README.md`](mspm0g3507_freertos/README.md) |
| `mspm0g3507_app/` | 分层的 G3507 电机、传感器、协议和应用组合工程 | [`README.md`](mspm0g3507_app/README.md) |
| `keil/mspm0g3507_app/` | G3507 应用的 Keil MDK 构建和调试工程 | [`README.md`](keil/mspm0g3507_app/README.md) |
| `stm32h723_app/` | STM32H723ZGT6 的 CubeMX、FreeRTOS、UART8 VOFA 健康遥测和 FDCAN 基础工程 | [`README.md`](stm32h723_app/README.md) |

## 文档地图

| 文档 | 职责 |
| --- | --- |
| [`docs/AI_HANDOFF.md`](docs/AI_HANDOFF.md) | AI 必读规则、当前状态、验证结果、硬件事实和遗留风险 |
| [`docs/DEPENDENCIES.md`](docs/DEPENDENCIES.md) | 软件版本、工具链、SDK、源码和硬件依赖的权威来源 |
| [`docs/SETUP.md`](docs/SETUP.md) | 跨电脑安装、环境变量、CCS、VS Code、Keil 和构建操作 |
| [`docs/CODING_STYLE.md`](docs/CODING_STYLE.md) | C 注释、接口、算法、驱动、FreeRTOS 任务和文档写作规范 |

## AI 首次接手顺序

新 AI 在查看、修改、构建或调试前，必须按以下顺序阅读：

1. 根目录 `README.md`，了解项目地图和安全边界；
2. `docs/AI_HANDOFF.md`，了解当前分支、已知事实、验证结果和遗留风险；
3. `docs/DEPENDENCIES.md`，确认工具链、SDK、CMSIS-Pack 和源码依赖；
4. `docs/SETUP.md`，在需要配置环境或构建时使用；
5. `docs/CODING_STYLE.md`，在修改 C 代码前确认接口和注释规范；
6. 与任务对应的工程 README；涉及 Keil 时追加读取 `keil/mspm0g3507_app/README.md`；
7. 最后再读取源码、工程文件和测试脚本。

## 开发完成后的文档更新

每个开发任务结束时，必须更新 `docs/AI_HANDOFF.md`，记录实际改动、验证命令、验证
结果、硬件事实和未完成风险。其他文档按变更类型更新：

| 变更类型 | 需要更新的文档 |
| --- | --- |
| 功能、接口、引脚或硬件行为变化 | `docs/AI_HANDOFF.md` 和对应工程 README |
| 工具链、SDK、CMSIS-Pack 或版本变化 | `docs/AI_HANDOFF.md`、`docs/DEPENDENCIES.md`、必要时 `docs/SETUP.md` |
| 构建脚本、环境变量或构建入口变化 | `docs/AI_HANDOFF.md`、`docs/SETUP.md`、受影响工程 README |
| C 注释或接口文档规范变化 | `docs/CODING_STYLE.md` |
| 目录职责、工程入口或 AI 流程变化 | 根目录 `README.md` 和 `docs/AI_HANDOFF.md` |

完成更新后还要检查 Markdown 链接、代码围栏、旧路径和说明性英文，并明确列出未执行
的 Flash、烧录、GDB/SWD、电机调试和硬件验收操作。

## 安全边界

构建、SysConfig 生成、主机测试和静态检查不会自动完成硬件验收。除非用户明确授权，
不得执行 Flash 擦除、烧录、探针枚举、GDB 服务、会暂停运行中电机的调试或其他实物
验收操作。G3507 与 L1306 的 SysConfig、设备包、linker、CMSIS-Pack 和引脚配置不得
混用。

详细版本基线和构建命令分别见 [`docs/DEPENDENCIES.md`](docs/DEPENDENCIES.md) 与
[`docs/SETUP.md`](docs/SETUP.md)。
