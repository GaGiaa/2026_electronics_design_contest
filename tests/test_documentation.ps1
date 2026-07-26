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

function Resolve-ProjectPath {
    param([Parameter(Mandatory)] [string] $RelativePath)

    return (Join-Path $ProjectRoot ($RelativePath -replace '/', '\'))
}

$markdownFiles = @(
    'README.md',
    'docs/AI_HANDOFF.md',
    'docs/DEPENDENCIES.md',
    'docs/SETUP.md',
    'docs/CODING_STYLE.md',
    'mspm0g3507_app/README.md',
    'mspm0g3507_freertos/README.md',
    'mspm0g3507_bringup/README.md',
    'mspm0l1306_bringup/README.md',
    'keil/mspm0g3507_app/README.md'
)

$texts = @{}
foreach ($relativePath in $markdownFiles) {
    $path = Resolve-ProjectPath $relativePath
    Assert-Condition (Test-Path -LiteralPath $path) "文档缺失：$relativePath"

    $lines = @(Get-Content -LiteralPath $path -Encoding UTF8)
    $texts[$relativePath] = ($lines -join "`n")

    $fenceLines = @($lines | Where-Object { $_ -match '^(```|~~~)' })
    Assert-Condition (($fenceLines.Count % 2) -eq 0) "代码围栏未闭合：$relativePath"
    Assert-Condition (-not ($lines | Where-Object { $_ -match '^~~~' })) "代码围栏必须使用标准反引号：$relativePath"

    $documentPath = Split-Path -Parent $path
    foreach ($line in $lines) {
        $linkMatches = [regex]::Matches($line, '\[[^\]]+\]\(([^)]+)\)')
        foreach ($linkMatch in $linkMatches) {
            $target = $linkMatch.Groups[1].Value
            if ($target -match '^(https?|mailto):' -or $target.StartsWith('#')) {
                continue
            }

            $targetPath = ($target -split '#', 2)[0]
            if ([string]::IsNullOrWhiteSpace($targetPath)) {
                continue
            }

            $resolvedTarget = Join-Path $documentPath ($targetPath -replace '/', '\')
            Assert-Condition (Test-Path -LiteralPath $resolvedTarget) "Markdown 链接失效：$relativePath -> $target"
        }
    }
}

$rootText = $texts['README.md']
$handoffText = $texts['docs/AI_HANDOFF.md']
Assert-Condition ($rootText -match 'AI') 'Root README is missing the AI handoff order.'
Assert-Condition ($rootText -match 'docs/AI_HANDOFF\.md') 'Root README is missing documentation update links.'
Assert-Condition ($handoffText -match 'AI ') 'AI_HANDOFF is missing the AI handoff section.'
Assert-Condition ($handoffText -match 'docs/DEPENDENCIES\.md') 'AI_HANDOFF is missing documentation ownership links.'
Assert-Condition ($handoffText -notmatch '40510de') 'AI_HANDOFF contains an obsolete migration commit.'
Assert-Condition ($texts['mspm0g3507_app/README.md'] -match 'APP_LINE_CONTROL_VOFA_TELEMETRY_ENABLE') 'G3507 README is missing line control VOFA configuration.'
Assert-Condition ($texts['mspm0g3507_app/README.md'] -match 'APP_LINE_CONTROL_CHANNEL_COUNT') 'G3507 README is missing line control VOFA channel count.'
Assert-Condition ($handoffText -match 'APP_LINE_CONTROL_VOFA_TELEMETRY_ENABLE') 'AI_HANDOFF is missing line control VOFA telemetry documentation.'
Assert-Condition ($texts['mspm0g3507_app/README.md'] -match 'APP_BUTTON_VOFA_TELEMETRY_ENABLE') 'G3507 README is missing button VOFA telemetry configuration.'
Assert-Condition ($handoffText -match 'app_state_buttons_publish') 'AI_HANDOFF is missing button input snapshot documentation.'
Assert-Condition ($handoffText -notmatch 'APP_BUTTON_FEATURE_ENABLE|key,pa7=down|key,pa7=up') 'Documentation contains the removed button text telemetry design.'

foreach ($relativePath in $markdownFiles) {
    Assert-Condition ($texts[$relativePath] -notmatch 'motor_pid') "文档包含失效 motor_pid 路径：$relativePath"
}

$freertosText = $texts['mspm0g3507_freertos/README.md']
foreach ($phrase in @('This is an independent', 'The application uses', 'Import the folder', 'Use the supplied', 'The output is')) {
    Assert-Condition ($freertosText -notmatch [regex]::Escape($phrase)) "FreeRTOS README 仍包含英文段落：$phrase"
}

Write-Host 'PASS: documentation structure, links, language, and stale-path checks passed.'
