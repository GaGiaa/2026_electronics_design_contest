[CmdletBinding()]
param(
    [string] $ProjectRoot,
    [string] $SdkRoot,
    [string] $SysConfigRoot,
    [string] $Compiler,
    [string] $CcsRoot,
    [string] $KeilRoot,
    [ValidateSet(0, 1)]
    [int] $VofaSpeedPidTelemetryEnable,
    [ValidateSet(0, 1)]
    [int] $ImuTelemetryEnable,
    [ValidateSet(0, 1)]
    [int] $ImuYawEnable,
    [ValidateSet(0, 1)]
    [int] $GrayVofaTelemetryEnable,
    [ValidateSet(1, 2)]
    [int] $EncoderDecodeMode
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

. (Join-Path $PSScriptRoot 'toolchain.ps1')
$toolchain = Get-ToolchainConfig -SdkRoot $SdkRoot -SysConfigRoot $SysConfigRoot -Compiler $Compiler -CcsRoot $CcsRoot -KeilRoot $KeilRoot
Assert-ToolchainConfig -Config $toolchain -Required @('SdkRoot', 'SysConfigRoot', 'KeilRoot', 'Uv4')
$SdkRoot = $toolchain.SdkRoot
$SysConfigRoot = $toolchain.SysConfigRoot
$KeilRoot = $toolchain.KeilRoot

$projectDir = Join-Path $ProjectRoot 'keil\mspm0g3507_app'
$projectTemplate = Join-Path $projectDir 'mspm0g3507_app.uvprojx'
$projectFileGenerator = Join-Path $PSScriptRoot 'generate-keil-project.ps1'
$generator = Join-Path $PSScriptRoot 'generate-keil-mspm0g3507-sysconfig.ps1'
$uv4 = $toolchain.Uv4
$objects = Join-Path $projectDir 'Objects'
$log = Join-Path $objects 'build.log'
$axf = Join-Path $objects 'mspm0g3507_app.axf'
$hex = Join-Path $objects 'mspm0g3507_app.hex'
$map = Join-Path $projectDir 'mspm0g3507_app.map'

foreach ($path in @($projectTemplate, $projectFileGenerator, $generator, $uv4)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required Keil build path is unavailable: $path"
    }
}

$protectedPaths = @(
    (Join-Path $ProjectRoot 'mspm0g3507_app\.cproject'),
    (Join-Path $ProjectRoot 'mspm0g3507_app\.project'),
    (Join-Path $ProjectRoot 'mspm0g3507_app\mspm0g3507_app.syscfg'),
    (Join-Path $ProjectRoot 'tools\build-mspm0g3507-app.ps1')
)
$protectedPaths += Get-ChildItem -LiteralPath (Join-Path $ProjectRoot '.vscode') -File -ErrorAction Stop | Select-Object -ExpandProperty FullName
$beforeHashes = @{}
foreach ($path in $protectedPaths) {
    $beforeHashes[$path] = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash
}

$projectFile = & $projectFileGenerator -ProjectRoot $ProjectRoot -SdkRoot $SdkRoot -SysConfigRoot $SysConfigRoot
if ([string]::IsNullOrWhiteSpace($projectFile) -or -not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
    throw 'Keil project generation failed.'
}

$projectFileContent = $null
$projectFileBytes = $null
$temporaryDefines = @()
if ($PSBoundParameters.ContainsKey('VofaSpeedPidTelemetryEnable')) {
    $temporaryDefines += "VOFA_SPEED_PID_TELEMETRY_ENABLE=$VofaSpeedPidTelemetryEnable"
}
if ($PSBoundParameters.ContainsKey('ImuTelemetryEnable')) {
    $temporaryDefines += "IMU_TELEMETRY_ENABLE=$ImuTelemetryEnable"
}
if ($PSBoundParameters.ContainsKey('ImuYawEnable')) {
    $temporaryDefines += "IMU_YAW_ENABLE=$ImuYawEnable"
}
if ($PSBoundParameters.ContainsKey('GrayVofaTelemetryEnable')) {
    $temporaryDefines += "GRAY_VOFA_TELEMETRY_ENABLE=$GrayVofaTelemetryEnable"
}
if ($PSBoundParameters.ContainsKey('EncoderDecodeMode')) {
    $temporaryDefines += "BOARD_ENCODER_DECODE_MODE=$EncoderDecodeMode"
}
if ($temporaryDefines.Count -gt 0) {
    $projectFileBytes = [System.IO.File]::ReadAllBytes($projectFile)
    $projectFileContent = Get-Content -Raw -Encoding UTF8 -LiteralPath $projectFile
    $projectFileWithDefines = $projectFileContent -replace '<Define>__MSPM0G3507__</Define>', "<Define>__MSPM0G3507__,$($temporaryDefines -join ',')</Define>"
    if ($projectFileWithDefines -eq $projectFileContent) {
        throw 'Keil project does not have the expected application define block.'
    }
    [System.IO.File]::WriteAllText(
        $projectFile,
        $projectFileWithDefines,
        (New-Object System.Text.UTF8Encoding($false)))
}

try {
    & $generator -ProjectRoot $ProjectRoot -SdkRoot $SdkRoot -SysConfigRoot $SysConfigRoot
    if ($LASTEXITCODE -ne 0) {
        throw 'Keil SysConfig generation failed.'
    }

    New-Item -ItemType Directory -Force -Path $objects | Out-Null
    Push-Location $projectDir
    try {
        & $uv4 -b $projectFile -j0 -o $log
        if ($LASTEXITCODE -ne 0) {
            throw "Keil UV4 build failed (exit code $LASTEXITCODE). See $log"
        }
    }
    finally {
        Pop-Location
    }
}
finally {
    if ($null -ne $projectFileBytes) {
        [System.IO.File]::WriteAllBytes($projectFile, $projectFileBytes)
    }
}

foreach ($path in @($axf, $hex, $map)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Keil build did not produce the expected output: $path"
    }
}

foreach ($path in $protectedPaths) {
    $afterHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash
    if ($afterHash -ne $beforeHashes[$path]) {
        throw "Protected CCS or VS Code file changed during Keil build: $path"
    }
}

Write-Output "Keil build succeeded: $axf"
