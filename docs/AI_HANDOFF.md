# MSPM0 核心板工程交接索引

最后更新：2026-07-25

## 使用规则

任何 AI 在查看、修改、构建、调试或烧录本仓库前，必须完整阅读本文档和根目录
`README.md`。任务完成后必须更新实际改动、验证结果、硬件事实和遗留风险。说明性正文
使用中文；代码标识符、命令、路径、协议名称、芯片型号和工具名称保持原样。

除非用户明确授权，不得执行 Flash 擦除、烧录、探针枚举、GDB 服务或会暂停运行中电机的
调试操作。不得混用 L1306 与 G3507 的 SysConfig、设备包、CMSIS-Pack、linker 或引脚配置。

## 文档职责与 AI 接手顺序

项目总览、工程地图和任务结束后的更新决策表见根目录 `README.md`。其他文档职责如下：

- `docs/AI_HANDOFF.md`：当前状态、硬件事实、验证结果、遗留风险和 AI 操作规则；
- `docs/DEPENDENCIES.md`：软件版本、工具链、源码依赖和硬件依赖的唯一权威来源；
- `docs/SETUP.md`：跨电脑环境配置、工具安装、构建和调试操作；
- `docs/CODING_STYLE.md`：C 注释、接口、算法、驱动、FreeRTOS 任务和文档写作规范；
- 各工程 `README.md`：对应工程的功能、接线、构建输出和工程级调试说明。

新 AI 必须按以下顺序阅读：

1. 根目录 `README.md`；
2. 本文档；
3. `docs/DEPENDENCIES.md`；
4. `docs/SETUP.md`；
5. `docs/CODING_STYLE.md`；
6. 与任务对应的工程 README；
7. 源码、工程文件和测试脚本。

涉及 Keil 构建时，还必须读取 `keil/mspm0g3507_app/README.md`。

## 当前 Git 状态

当前工作区分支为 `develop_1`，远端跟踪分支已不存在。开始本轮文档整理时，HEAD 为
`24b4073`，工作区干净；当前工作区包含本轮文档重构的未提交修改。后续应以实际
`git status` 和 `git log` 输出为准。

此前的功能合并基线按以下顺序完成：

- `d2a97dc`：关闭 VOFA 速度遥测默认开关；
- `af02c42`：加入 IMU Yaw 算法和 JustFloat 遥测；
- `8181750`：加入灰度线跟踪和灰度 VOFA 遥测；
- `7d98698`：加入编码器速度滤波、四轮速度 PID 和四通道速度遥测；
- `4d0277a`：完成 G3507 应用工程分层迁移；
- `24b4073`：整理工程文档体系并完成中文化。

提交号只能记录已经通过 `git log` 验证的历史事实，不得预先填写尚未创建的提交号。

## 工程与依赖索引

| 工程 | 芯片与用途 | 主要入口 |
| --- | --- | --- |
| `mspm0l1306_bringup/` | MSPM0L1306 初始调试 | `tools/build-mspm0l1306.ps1` |
| `mspm0g3507_bringup/` | MSPM0G3507 初始调试 | `tools/build-mspm0g3507.ps1` |
| `mspm0g3507_freertos/` | G3507 FreeRTOS 静态分配基线 | `tools/build-mspm0g3507-freertos.ps1` |
| `mspm0g3507_app/` | G3507 分层应用 | `tools/build-mspm0g3507-app.ps1` |
| `keil/mspm0g3507_app/` | G3507 应用 Keil MDK 工程 | `tools/build-keil-mspm0g3507-app.ps1` |

工具版本和完整源码依赖见 `docs/DEPENDENCIES.md`。跨电脑安装和构建步骤见
`docs/SETUP.md`。G3507 应用的功能、接线、遥测和接口说明见
`mspm0g3507_app/README.md`。

## 构建与安全边界

常用构建命令如下：

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-mspm0l1306.ps1
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507.ps1
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-freertos.ps1
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-app.ps1
powershell -ExecutionPolicy Bypass -File tools\build-keil-mspm0g3507-app.ps1
```

`app_state.h` 公开 `g_encoder_sample_sequence`、`g_grayscale_publish_sequence` 和
`g_drive_control_publish_sequence` 供 SWD 观察；启用 `APP_IMU_YAW_ENABLE=1U` 时还公开
`g_imu_yaw_snapshot` 和 `g_imu_yaw_publish_sequence`。这些 `volatile` 变量是快照序列保护状态，
只应观察，不应通过调试器写入。灰度驱动的 `volatile g_grayscale_debug` 是标定参数和数字状态的只读镜像，
不改变驱动内部仍保持 `static` 的标定数组。任务栈、通信缓存、编码器滤波器和 PID 内部状态继续保持私有链接。

G3507 CCS 应用输出为 `mspm0g3507_app/Debug/mspm0g3507_app.out`。Keil 输出为
`keil/mspm0g3507_app/Objects/mspm0g3507_app.axf`、对应 HEX 和
`keil/mspm0g3507_app/mspm0g3507_app.map`。

`mspm0g3507_app` 的 CCS 工程用于源码浏览、SysConfig 和调试目标配置；由于 FreeRTOS
内核源码来自本机 SDK，跨电脑可复现构建必须使用 PowerShell 构建脚本。全新 CCS
workspace 直接执行 `Project -> Build Project` 不是该应用的完整构建入口。

构建、SysConfig 生成、主机测试、静态检查和 GDB server 不会写入 Flash。任何会执行
`--erase chip`、下载 firmware 或连接目标板的操作，都必须获得用户本次操作的明确授权。

## 当前硬件事实

### G3507 电机与编码器

- CPU 为 80 MHz，四路 PWM 为 10 kHz；详细轮位引脚见 `mspm0g3507_app/README.md`；
- 电机轴编码器为 13 线，减速比 20:1，输出轴机械线数为 260；
- 默认 A 相双沿模式为每圈 520 counts，AB 正交 X4 模式为每圈 1040 counts；
- 更换电机、接线或解码模式后，必须低速手动确认方向、单圈计数和四轮隔离；
- 电机运动时不得设置断点，初次调试应让车轮悬空或断开电机电源。

### BMI160 与 IMU Yaw

BMI160 使用 SPI0：`SCK=PB18`、`MOSI=PB17`、`MISO=PB19`、`CS=PB0`，配置为
100 Hz、±4g、±500 dps。驱动已确认 `CHIP_ID=0xD1`，静止时 Z 轴加速度约为 1g，
静止陀螺仪输出接近 0。IMU Yaw 使用静止样本估计零偏并融合编码器差速，不能提供绝对
航向；长期零偏、漂移、方向符号和急转弯响应仍属于待完成硬件验收。

### 灰度、WS2812、按键和 OLED

八路灰度传感器使用 74HC4051：`AD0=PB13`、`AD1=PB1`、`AD2=PB23`、`OUT=PA27`。
灰度结果通过 `g_grayscale_snapshot` 提供 SWD 和 VOFA 观察，并由高档循迹模式读取其
`line_error`、`line_strength`、`sequence` 和 `adc_timeout_mask`。WS2812 使用 SPI1 输出到
PB22；四针 SSD1306 OLED 使用 I2C0：`SDA=PA0`、`SCL=PA1`，默认地址为 `0x3C`。
WS2812、蜂鸣器、按键和 OLED 的实物响应仍需单独验收。

### UART 遥测与 CRSF

UART0 同一时间只能运行一种遥测模式。速度 VOFA、灰度 VOFA 和 IMU Yaw 在编译期互斥，
启用遥测时不创建 UART 回显任务。当前 CRSF 映射为 CH3（索引 2）控制前进和后退，
CH1（索引 0）控制差速转向，SB/CH5 使用通道数组索引 6 控制低档空闲、中档手动和高档循迹；连续
100 ms 没有有效帧时四轮目标清零。`CRSF_REMOTE_CONTROL_ENABLE` 默认值为 `1U`，
仍可通过构建参数设为 `0` 以保留无 CRSF 的 SWD 单轮电机调试路径。

详细帧格式、构建参数和调试变量见 `mspm0g3507_app/README.md`。CRSF 硬件验收前必须
让车轮悬空或断开电机电源。

### 黑线循迹闭环

新增 `algorithms/line_control/`，使用现有 `PID_Position` 将灰度位置误差转换为左右
差速速度，再由四轮速度 PID 生成 PWM。默认位置式 PID 为 `kp=0.08f`、`ki=0`、`kd=0`，
转向输出上限为 300 mm/s。`g_line_control_debug` 提供 SWD 可写的 PID、轮速、转向符号、
灰度阈值和丢线时间参数；`g_drive_control_snapshot` 通过 `app_state` 提供模式、SB、
灰度、PID、四轮目标和反馈速度的序列保护快照。

线控要求 `sequence` 非零、ADC 超时掩码为 0 且 `line_strength` 达到进入阈值 800；已有效
后使用退出阈值 400。丢线时冻结 PID 并保持上次转向输出 100 ms，之后清零目标并复位。
本轮没有新增 VOFA 帧，已有灰度、速度 PID 和 IMU JustFloat 帧格式保持不变。

## 分层迁移事实

`mspm0g3507_app` 的规范实现位于 `app/`、`drivers/`、`algorithms/`、`protocols/`、
`services/`、`config/` 和 `platform/`。分层迁移遗留的根目录兼容头文件和 PID 旧目录的
转发头文件已经全部移除；新代码和测试必须直接引用规范路径。CCS 和 Keil 工程中每个
实现文件只加入一次。

应用配置由 `config/app_config.h` 统一维护，应用级编译宏使用 `APP_` 前缀；
`app/app_profile.h` 只保留 `app_profile_t` 和 `app_profile_get()`。编码器机械参数与解码模式
位于 `config/encoder_config.h`，RTOS monitor 资源和计时参数位于
`config/rtos_monitor_config.h`。`config/FreeRTOSConfig.h` 从 monitor 配置派生
`configMAX_TASK_NAME_LEN`，不再与服务头重复定义默认值。现有 PowerShell 构建参数名保持不变，
但直接传入旧的应用级 `-D` 宏不再是支持的接口。

`main.c` 只负责 SysConfig 初始化、调用 `app_startup()`、启动 FreeRTOS 调度器和异常
停机处理。应用任务注册接口为 `app_startup()`、`app_tasks_motor_start()`、
`app_tasks_sensor_start()`、`app_tasks_io_start()` 和 `app_tasks_telemetry_start()`。

现有公开函数名、结构体名、PowerShell 构建参数、SWD 全局变量、UART 行为和 CRSF 超时行为保持不变；
应用源码级配置宏已统一为 `APP_` 前缀。当前 `APP_IMU_YAW_ENABLE` 和 `APP_RTOS_MONITOR_ENABLE` 默认开启，
仍可分别通过 `-ImuYawEnable 0` 和 `-RtosMonitorEnable 0` 显式关闭。
分层迁移不代表任何尚未完成的硬件验收已经完成。

IMU yaw 与 IMU VOFA 遥测采用独立任务边界：`APP_IMU_YAW_ENABLE=1U` 时创建 `imu_task`，负责
BMI160 初始化、采样、失败重试和 yaw 融合；`APP_IMU_TELEMETRY_ENABLE=1U` 时创建独立的
`imu_vofa_task`。两个任务通过 `app_state` 的 `app_imu_yaw_snapshot_t` 快照接口通信，快照包含
三个 yaw 浮点值、`valid` 和序列号，并使用序列保护跨任务复制。无效采样期间不发送 VOFA 帧。
`APP_IMU_VOFA_TELEMETRY_INTERVAL_MS` 与 `APP_IMU_VOFA_TELEMETRY_TASK_STACK_DEPTH` 分别配置
独立遥测周期和栈大小；`ImuYawEnable`、`ImuTelemetryEnable` 等既有 PowerShell 参数继续保留。
由于该方案明确采用 yaw 开关控制 IMU 任务，关闭 `APP_IMU_YAW_ENABLE` 时不会初始化或读取 BMI160，
同时 `app_profile_t.enable_imu` 为 `false`；当前默认值为 `1U`，因此默认构建会创建 IMU 任务。

## 已知验证与遗留风险

### 本轮 VS Code IntelliSense 修复

- 修复 `.vscode/settings.json` 只包含 `mspm0l1306_bringup` 头文件路径的问题，补齐四套 CCS 工程、各自 `Debug` 生成目录、SDK FreeRTOS、TI Arm Clang FreeRTOS port 和 CMSIS 路径。
- 补充 G3507 应用当前构建使用的 `__MSPM0G3507__`、`__USE_SYSCONFIG__` 和 Cortex-M0+ 编译参数，避免 CMSIS 被主机默认架构错误解析。
- 新增 `tests/test_vscode_intellisense.ps1`，检查 VS Code 配置路径、宏、编译参数和跨电脑路径可移植性。
- 根工作区的 C/C++ IntelliSense 默认面向当前主线 `mspm0g3507_app`。L1306 与 G3507 不能共用同一组芯片宏；单独检查 L1306 时应在对应工程目录或独立 VS Code 配置中选择 `__MSPM0L1306__`。
- 已验证 VS Code 配置 JSON、配置回归测试，以及使用 TI Arm Clang 对 G3507 应用入口和黑线控制源文件的 Cortex-M0+ 语法检查。
- 本轮未执行 Flash 擦除、烧录、探针枚举、GDB/SWD、电机调试或任何实物验收操作。

### 本轮 WS2812 状态指示功能

- 在 `mspm0g3507_app/config/app_config.h` 增加 `APP_WS2812_ANIMATION_ENABLE` 和
  `APP_WS2812_STATUS_INDICATOR_ENABLE`。旧的四灯轮流动画默认关闭，新的状态指示默认开启，
  两种流程不能同时启用。
- `ws2812_task` 保持单任务输出：1 号灯根据 `app_state` 中的 CRSF 链路快照显示绿灯或
  500 ms 红灯闪烁，4 号灯每 500 ms 按红、蓝、绿、白换色，2、3 号灯保持熄灭。
- 已通过 `powershell -ExecutionPolicy Bypass -File tests\test_mspm0g3507_app.ps1`。
- 已通过 `powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-app.ps1`；构建过程只生成
  `Debug` 软件产物，未执行 Flash、烧录、GDB/SWD、电机调试或 WS2812 实物验收。

此前已记录通过的验证包括电机控制、编码器模式 1/2、线跟踪、IMU Yaw、CRSF、VOFA
JustFloat、G3507 应用静态集成、OLED、RTOS monitor、CCS/Keil 工程静态检查和可移植性
检查。本轮额外增加 IMU yaw app-state host test，并通过 `test_config_ownership.ps1`、`test_config_validation.ps1`、RTOS monitor
host test、CRSF、电机控制、舵机、IMU yaw、线跟踪和 VOFA 单元测试。TI Clang 与 Keil
构建结果只能说明软件构建链路通过。

本轮闭环开发已通过 `test_line_control.ps1`、`test_crsf.ps1`、`test_line_tracking.ps1`、
`test_motor_control.ps1` 和 `test_app_state_imu_yaw.ps1`。TI Clang 应用构建已分别通过默认
`CRSF_REMOTE_CONTROL_ENABLE=1` 和 `-CrsfRemoteControlEnable 0` 配置；Keil 软件工程构建也已
通过。本轮未执行 Flash、GDB、烧录、电机调试或任何循迹实物验收。

本轮未执行 Flash 擦除、烧录、探针枚举、电机调试或任何实物验收操作。

尚未完成或需要持续复核的硬件项目包括：

- BMI160 长期零偏、静止漂移、左右转 yaw 符号和急转弯响应；
- 编码器四轮方向、单圈计数、解码模式和低速隔离；
- 四轮速度 PID 低速调参、滤波延迟和 PWM 抖动；
- 灰度白黑归一化、位图、横向误差、全白、全黑和丢线状态；
- 黑线循迹位置式 PID 的 `turn_sign`、基础速度、四轮速度环联调和模式切换实车响应；
- VOFA+ 曲线数量、通道顺序、帧尾和 UART 实际接收；
- WS2812、蜂鸣器、按键和 OLED 实物响应。

## 任务完成清单

每个开发任务结束时，必须：

1. 更新本文档中的实际改动、验证命令、验证结果、硬件事实和遗留风险；
2. 功能、接口、引脚或硬件行为变化时，更新对应工程 README；
3. 工具链、SDK、CMSIS-Pack 或版本变化时，更新 `docs/DEPENDENCIES.md`，必要时更新
   `docs/SETUP.md`；
4. 构建入口、目录职责或 AI 接手流程变化时，更新根目录 `README.md`；
5. 注释规范变化时，更新 `docs/CODING_STYLE.md`；
6. 检查 Markdown 相对链接、代码围栏、失效路径和说明性英文；
7. 明确列出未执行的 Flash、烧录、GDB/SWD、电机调试和硬件验收操作。

## Git 操作授权规则

除非用户明确要求，AI 不得自行执行 `git commit`、`git push`、创建 Pull Request 或向远程
仓库发布内容。普通开发任务只允许修改工作区并运行必要的本地验证。

用户明确要求执行 `git commit` 时，提交信息必须使用详细中文，说明改动目的、主要内容
和验证结果；不得只使用笼统的英文标题或过于简短的提交说明。
