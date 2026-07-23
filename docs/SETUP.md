# 跨电脑环境配置

本仓库包含 CCS 工程、PowerShell 命令行构建脚本、VS Code 任务和 Keil MDK
工程。源码路径均使用仓库相对路径。TI SDK、SysConfig、CCS、Keil、Arm GNU
GDB、Python、pyOCD 和 CMSIS-Pack 都属于每台电脑需要单独安装的本地依赖。

## 所需版本

- CCS 20.2，或与工程兼容的版本
- MSPM0 SDK 2.11.00.07
- SysConfig 1.26.2
- TI Arm Clang 4.0.3.LTS
- Keil MDK 5.43a，Keil 构建需要 Arm Compiler 6.24
- Python 3，用于配置 pyOCD
- pyOCD `>=0.45,<0.46`

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
$env:MSPM0_SDK_ROOT = 'C:\path\to\mspm0_sdk_2_11_00_07'
$env:SYSCONFIG_ROOT = 'C:\path\to\sysconfig_1.26.2'
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

构建命令不会擦除或写入 Flash。Flash 烧录必须通过单独的 pyOCD 或 Keil 操作
明确执行，并且执行前需要确认已连接的目标板。

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

如果 CCS Build Console 出现类似
`D:\ti\mspm0_sdk_2_10_00_04` 或 `sysconfig_1.28.0`，说明当前 CCS
workspace 绑定了旧产品。安装并选择 MSPM0 SDK `2.11.00.07`、SysConfig
`1.26.2`，删除旧 workspace 中的同名工程后，在全新的 workspace 中重新导入；
然后确认构建命令使用当前 clone 的项目目录。不要把该机器上的绝对路径提交到
Git。

请根据 `.vscode/extensions.json` 安装推荐的 VS Code 扩展，然后在 VS Code 中打开
仓库根目录。构建任务调用 PowerShell 脚本；调试任务需要先启动 pyOCD GDB
服务器，并正确配置 `ARM_GDB_PATH`。

## 团队 Git 协作

使用 `git clone` 克隆远程仓库。不要复制或提交链接 worktree 中的 `.git` 文件，
因为 worktree 的 `.git` 文件指向原电脑上的 Git 元数据目录。`Debug`、`Generated`、
`Objects`、`tools/.venv`、`tools/packs` 和本地 Keil 工程文件均为本机生成文件，
已加入 Git 忽略规则，不应提交到仓库。
