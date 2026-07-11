
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$outputDir = Join-Path $repoRoot ".Temp"
$outputPath = Join-Path $outputDir "all-agents-combined.md"

function Get-RepositoryRelativePath {
    param([Parameter(Mandatory)][string]$Path)

    $relativePath = $null
    $method = [System.IO.Path].GetMethod(
        "GetRelativePath",
        [Type[]]@([string], [string])
    )

    if ($null -ne $method) {
        $relativePath = [System.IO.Path]::GetRelativePath($repoRoot, $Path)
    }
    else {
        # Windows PowerShell 5.1 / .NET Framework fallback.
        $basePath = [System.IO.Path]::GetFullPath($repoRoot)
        $targetPath = [System.IO.Path]::GetFullPath($Path)

        if (-not $basePath.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
            $basePath += [System.IO.Path]::DirectorySeparatorChar
        }

        $baseUri = New-Object System.Uri($basePath)
        $targetUri = New-Object System.Uri($targetPath)
        $relativePath = [System.Uri]::UnescapeDataString(
            $baseUri.MakeRelativeUri($targetUri).ToString()
        )
    }

    return $relativePath.Replace("\", "/")
}

function Sort-RepositoryPaths {
    param([string[]]$Paths)

    [Array]::Sort($Paths, [System.StringComparer]::Ordinal)
    return $Paths
}

$requiredRoots = @("AGENTS.md", ".github/AGENTS.md")
foreach ($relativePath in $requiredRoots) {
    $path = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required governance file not found: $relativePath"
    }
}

$agentsDirectory = Join-Path $repoRoot ".github/agents"
if (-not (Test-Path -LiteralPath $agentsDirectory -PathType Container)) {
    throw "Required custom agents directory not found: .github/agents/"
}

$customAgents = @(
    Get-ChildItem -LiteralPath $agentsDirectory -File -Filter "*.agent.md" |
        ForEach-Object { Get-RepositoryRelativePath $_.FullName }
)
if ($customAgents.Count -eq 0) {
    throw "Required custom agent pattern not found: .github/agents/*.agent.md"
}
$customAgents = Sort-RepositoryPaths $customAgents

$skillsDirectory = Join-Path $repoRoot ".agents/skills"
$skills = @()
if (Test-Path -LiteralPath $skillsDirectory -PathType Container) {
    $skills = @(
        Get-ChildItem -LiteralPath $skillsDirectory -File -Recurse -Filter "SKILL.md" |
            ForEach-Object { Get-RepositoryRelativePath $_.FullName }
    )
    $skills = Sort-RepositoryPaths $skills
}

$optionalReferences = @()
$runtimeReference = "docs/governance-agent-runtime.md"
if (Test-Path -LiteralPath (Join-Path $repoRoot $runtimeReference) -PathType Leaf) {
    $optionalReferences = @($runtimeReference)
}

$includedFiles = @($requiredRoots) + @($customAgents) + @($skills) + @($optionalReferences)

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$content = New-Object System.Text.StringBuilder
[void]$content.AppendLine("# Combined Agent Governance")
[void]$content.AppendLine()
[void]$content.AppendLine("Generated file. Do not edit; run tools/combine.agent.md.ps1 to regenerate it.")
[void]$content.AppendLine("Sources are repository-relative paths in the manifest below.")
[void]$content.AppendLine()
[void]$content.AppendLine("## Summary")
[void]$content.AppendLine()
[void]$content.AppendLine("- Governance roots: $($requiredRoots.Count)")
[void]$content.AppendLine("- Custom agents: $($customAgents.Count)")
[void]$content.AppendLine("- Skills: $($skills.Count)")
[void]$content.AppendLine("- Optional reference documents: $($optionalReferences.Count)")
[void]$content.AppendLine()
[void]$content.AppendLine("## Manifest")
[void]$content.AppendLine()
foreach ($relativePath in $includedFiles) {
    [void]$content.AppendLine(("- ``{0}``" -f $relativePath))
}
[void]$content.AppendLine()

foreach ($relativePath in $includedFiles) {
    $path = Join-Path $repoRoot $relativePath

    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Included governance file not found: $relativePath"
    }

    [void]$content.AppendLine(("## ``{0}``" -f $relativePath))
    [void]$content.AppendLine()
    [void]$content.Append([System.IO.File]::ReadAllText($path))
    if ($content.Length -eq 0 -or $content[$content.Length - 1] -notin "`n", "`r") {
        [void]$content.AppendLine()
    }
    [void]$content.AppendLine()
    [void]$content.AppendLine("---")
    [void]$content.AppendLine()
}

[System.IO.File]::WriteAllText(
    $outputPath,
    $content.ToString(),
    [System.Text.UTF8Encoding]::new($false)
)

Get-Item -LiteralPath $outputPath | Select-Object FullName, Length, LastWriteTime
