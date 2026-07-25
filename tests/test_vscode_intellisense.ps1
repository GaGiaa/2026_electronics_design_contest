[CmdletBinding()]
param(
    [string] $ProjectRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

function Assert-Condition {
    param(
        [Parameter(Mandatory)] [bool] $Condition,
        [Parameter(Mandatory)] [string] $Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

$settingsPath = Join-Path $ProjectRoot '.vscode\settings.json'
Assert-Condition (Test-Path -LiteralPath $settingsPath) 'VS Code settings.json is missing.'

$settings = Get-Content -LiteralPath $settingsPath -Raw -Encoding UTF8 | ConvertFrom-Json
$includePaths = @($settings.'C_Cpp.default.includePath')
$requiredPaths = @(
    '${workspaceFolder}/mspm0l1306_bringup',
    '${workspaceFolder}/mspm0l1306_bringup/Debug',
    '${workspaceFolder}/mspm0g3507_bringup',
    '${workspaceFolder}/mspm0g3507_bringup/Debug',
    '${workspaceFolder}/mspm0g3507_freertos',
    '${workspaceFolder}/mspm0g3507_freertos/Debug',
    '${workspaceFolder}/mspm0g3507_app',
    '${workspaceFolder}/mspm0g3507_app/Debug',
    '${env:MSPM0_SDK_ROOT}/kernel/freertos/Source/include',
    '${env:MSPM0_SDK_ROOT}/kernel/freertos/Source/portable/TI_ARM_CLANG/ARM_CM0',
    '${env:MSPM0_SDK_ROOT}/source',
    '${env:MSPM0_SDK_ROOT}/source/third_party/CMSIS/Core/Include'
)

foreach ($requiredPath in $requiredPaths) {
    Assert-Condition ($includePaths -contains $requiredPath) "VS Code includePath is missing: $requiredPath"
}

$defines = @($settings.'C_Cpp.default.defines')
foreach ($requiredDefine in @('__MSPM0G3507__', '__USE_SYSCONFIG__')) {
    Assert-Condition ($defines -contains $requiredDefine) "VS Code defines is missing: $requiredDefine"
}

Assert-Condition ($settings.'C_Cpp.default.compilerPath' -eq '${env:TI_ARM_CLANG}') 'VS Code must use TI_ARM_CLANG from the environment.'
$compilerArgs = @($settings.'C_Cpp.default.compilerArgs')
foreach ($requiredArgument in @('-mcpu=cortex-m0plus', '-march=thumbv6m', '-mfloat-abi=soft', '-mthumb')) {
    Assert-Condition ($compilerArgs -contains $requiredArgument) "VS Code compilerArgs is missing: $requiredArgument"
}
Assert-Condition (-not (($includePaths -join "`n") -match '(?i)([A-Z]:\\|/Users/|/home/)')) 'VS Code includePath contains a machine-specific absolute path.'

Write-Output 'VS Code IntelliSense configuration checks passed.'
