# 跨电脑环境配置

本仓库包含 CCS 工程、PowerShell 命令行构建脚本、VS Code 任务和 Keil MDK
工程。源码路径均使用仓库相对路径。TI SDK、SysConfig、CCS、Keil、Arm GNU
GDB、Python、pyOCD 和 CMSIS-Pack 都属于每台电脑需要单独安装的本地依赖。

相关文档职责如下：`docs/DEPENDENCIES.md` 是版本和依赖的唯一权威来源，
`docs/AI_HANDOFF.md` 记录当前工程状态和安全边界，`docs/CODING_STYLE.md` 记录 C 代码
和文档写作规范，根目录 `README.md` 记录项目地图和 AI 接手顺序。各工程的功能、接线
和调试说明见对应工程目录中的 `README.md`。

## 工具安装

具体版本和兼容范围以 `docs/DEPENDENCIES.md` 的“版本基线”章节为准。本机需要安装
CCS、MSPM0 SDK、SysConfig、TI Arm Clang、Keil MDK、Arm Compiler、Python 3、pyOCD
以及对应的 CMSIS-Pack。

请使用 Keil Pack Installer 安装 G3507 对应的 CMSIS-Pack。在仓库根目录执行
`tools\install-g3507-debug-tools.ps1`，配置本地 pyOCD 环境并下载 G3507 Pack。
使用 L1306 核心板时，执行 `tools\install-debug-tools.ps1`。

## 工具路径配置

首次配置可以直接在仓库根目录执行以下命令。脚本会自动查找已安装的工具，并将
路径保存到当前 Windows 用户环境变量中：

```powershell
powershell -ExecutionPolicy Bypass -File tools\configure-toolchain.ps1 -PersistUserEnvironment
```

执行后请重新打开 VS Code，使 IntelliSense 和 Cortex-Debug 读取新的环境变量。

可以在当前 PowerShell 会话中设置以下环境变量，也可以把对应参数传给构建脚本：

```powershell
$env:MSPM0_SDK_ROOT = 'C:\path\to\mspm0_sdk'
$env:SYSCONFIG_ROOT = 'C:\path\to\sysconfig'
$env:TI_ARM_CLANG = 'C:\path\to\tiarmclang.exe'
$env:CCS_ROOT = 'C:\path\to\ccs'
$env:KEIL_ROOT = 'C:\path\to\Keil_v5'
$env:ARM_GDB_PATH = 'C:\path\to\arm-none-eabi-gdb.exe'
```

构建脚本中显式传入的参数优先级高于环境变量。统一路径解析脚本为
`tools\toolchain.ps1`，它会根据已配置的工具根目录自动推导
`sysconfig_cli.bat`、`gmake.exe` 和 `UV4.exe` 的路径。

## 构建工程

在仓库根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-mspm0l1306.ps1
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507.ps1
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-freertos.ps1
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-app.ps1
powershell -ExecutionPolicy Bypass -File tools\build-keil-mspm0g3507-app.ps1
```

Keil 源文件 `keil\mspm0g3507_app\mspm0g3507_app.uvprojx` 是工程模板。
构建脚本和 VS Code 打开任务会根据当前 SDK 路径生成本地文件
`mspm0g3507_app.local.uvprojx`。该文件已被 Git 忽略。

构建命令不会擦除或写入 Flash。Flash 烧录必须通过单独的 pyOCD 或 Keil 操作明确执行，
并且执行前需要用户明确授权、确认目标板和确认目标芯片。

## CCS 与 VS Code

请在仓库外创建 CCS workspace，然后使用
`Project -> Import CCS Projects` 导入需要的工程目录。仓库中的 CCS 工程使用
CCS 产品变量，不依赖本机绝对源码路径。

`mspm0g3507_app` 的 FreeRTOS 内核源文件位于每台电脑本地安装的 MSPM0 SDK
中，仓库 CCS 元数据不包含这些外部源文件的完整编译输入。因此，导入后可用
CCS 编辑源码、修改 SysConfig 和配置调试目标，但不要直接使用
`Project -> Build Project` 作为该应用的跨电脑构建入口；否则常见结果是
`FreeRTOS.h` 或 `pid.h` 找不到，随后还会出现 FreeRTOS 内核符号未定义。
应用的可复现构建入口是：

```powershell
powershell -ExecutionPolicy Bypass -File tools\configure-toolchain.ps1 -PersistUserEnvironment
powershell -ExecutionPolicy Bypass -File tools\build-mspm0g3507-app.ps1
```

如果 CCS Build Console 出现旧 SDK 或旧 SysConfig 路径，说明当前 CCS workspace
绑定了旧产品。请按照 `docs/DEPENDENCIES.md` 的版本基线重新选择工具，删除旧 workspace
中的同名工程后，在全新的 workspace 中重新导入；然后确认构建命令使用当前 clone 的
项目目录。不要把该机器上的绝对路径提交到 Git。

请根据 `.vscode/extensions.json` 安装推荐的 VS Code 扩展，然后在 VS Code 中打开
仓库根目录。构建任务调用 PowerShell 脚本；调试任务需要先启动 pyOCD GDB
服务器，并正确配置 `ARM_GDB_PATH`。

仓库根目录的 `.vscode/settings.json` 已配置四套工程的源码、`Debug` 生成目录、SDK
FreeRTOS、CMSIS 和 TI Arm Clang Cortex-M0+ 参数，默认 IntelliSense 目标为当前主线的
`mspm0g3507_app`。由于 L1306 与 G3507 需要不同的芯片宏，单独检查 L1306 源码时不要
把 `__MSPM0L1306__` 和 `__MSPM0G3507__` 同时加入同一个配置；应切换到对应工程的独立
VS Code 配置。

## 文档维护

开发完成后必须先更新 `docs/AI_HANDOFF.md`。工具版本或环境变量变化时更新
`docs/DEPENDENCIES.md`；构建流程变化时更新本文档和受影响工程 README。文档正文使用
中文，命令、路径、函数名、宏名、协议名称和工具名称保持原样。

## 团队 Git 协作

使用 `git clone` 克隆远程仓库。不要复制或提交链接 worktree 中的 `.git` 文件，
因为 worktree 的 `.git` 文件指向原电脑上的 Git 元数据目录。`Debug`、`Generated`、
`Objects`、`tools/.venv`、`tools/packs` 和本地 Keil 工程文件均为本机生成文件，
已加入 Git 忽略规则，不应提交到仓库。
