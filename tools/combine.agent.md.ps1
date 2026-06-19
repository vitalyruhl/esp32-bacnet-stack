
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$outputDir = Join-Path $repoRoot ".Temp"
$outputPath = Join-Path $outputDir "all-agents-combined.md"

$governanceFiles = @(
    "AGENTS.md"
    ".github/AGENTS.md"
    ".github/agents/project.agent.md"
    ".github/agents/control-plane.agent.md"
    ".github/agents/docs.agent.md"
    ".github/agents/plan.agent.md"
    ".github/agents/refactor.agent.md"
    ".github/agents/workflow.agent.md"
)

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

if (Test-Path -LiteralPath $outputPath) {
    Remove-Item -LiteralPath $outputPath
}

$governanceFiles | ForEach-Object {
    $relativePath = $_
    $path = Join-Path $repoRoot $relativePath

    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required governance file not found: $relativePath"
    }

    "## $relativePath"
    ""
    Get-Content -LiteralPath $path
    ""
    "---"
    ""
} | Set-Content -LiteralPath $outputPath -Encoding UTF8

Get-Item -LiteralPath $outputPath | Select-Object FullName, Length, LastWriteTime
