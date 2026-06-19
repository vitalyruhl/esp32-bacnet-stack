<#
.SYNOPSIS
Exports GitHub Project issues to a raw Serena shared memory snapshot.

.DESCRIPTION
Reads GitHub Project metadata, project items, and linked issue details through
the GitHub CLI, then writes a markdown snapshot for temporary Serena memory use.
The output is raw planning state, not curated long-term memory.

Run from the repository root or any subdirectory. Relative output paths are
resolved from the Git repository root.

.PARAMETER ProjectNumber
GitHub Project number. If omitted, the script detects a project number from
.github/agents/project.agent.md text such as "(#6)".

.PARAMETER Owner
GitHub Project owner login. If omitted, the script derives the owner from
"gh repo view --json nameWithOwner".

.PARAMETER OutputPath
Output markdown path. Defaults to .serena/memories/shared/gh-project.md.

.PARAMETER Limit
Maximum project items to fetch. Defaults to 200.

.PARAMETER IncludeClosed
Include closed issues. By default, closed issues are omitted from the issue
detail section but the raw project item list is still captured.

.PARAMETER IssueNumbers
Manual fallback issue numbers. When provided, these issue numbers are exported
instead of deriving issue numbers from the project item list.

.PARAMETER DryRun
Generate and print the markdown snapshot summary without writing a file.

.EXAMPLE
.\tools\serena\export-gh-project-to-serena.ps1 -DryRun

.EXAMPLE
.\tools\serena\export-gh-project-to-serena.ps1

.EXAMPLE
.\tools\serena\export-gh-project-to-serena.ps1 -IssueNumbers 10,11,12 -DryRun

.EXAMPLE
.\tools\serena\export-gh-project-to-serena.ps1 -ProjectNumber 6 -Owner vitalyruhl -OutputPath .serena/memories/shared/gh-project.md
#>

[CmdletBinding()]
param(
    [int]$ProjectNumber,
    [string]$Owner,
    [string]$OutputPath,
    [int]$Limit = 200,
    [switch]$IncludeClosed,
    [int[]]$IssueNumbers,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $output = & $Command @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        $message = ($output | Out-String).Trim()
        throw "Command failed: $Command $($Arguments -join ' ')`n$message"
    }

    return ($output | Out-String).Trim()
}

function Invoke-GhJson {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $json = Invoke-External -Command "gh" -Arguments $Arguments
    if ([string]::IsNullOrWhiteSpace($json)) {
        throw "GitHub CLI returned empty JSON for: gh $($Arguments -join ' ')"
    }

    return $json | ConvertFrom-Json
}

function ConvertTo-JsonBlock {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Value,

        [int]$Depth = 20
    )

    return ($Value | ConvertTo-Json -Depth $Depth)
}

function Resolve-RepositoryRoot {
    $root = Invoke-External -Command "git" -Arguments @("rev-parse", "--show-toplevel")
    return (Resolve-Path -LiteralPath $root).Path
}

function Resolve-OutputFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepositoryRoot,

        [string]$RequestedPath
    )

    if ([string]::IsNullOrWhiteSpace($RequestedPath)) {
        return Join-Path $RepositoryRoot ".serena/memories/shared/gh-project.md"
    }

    if ([System.IO.Path]::IsPathRooted($RequestedPath)) {
        return $RequestedPath
    }

    return Join-Path $RepositoryRoot $RequestedPath
}

function Get-ProjectNumberFromProfile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepositoryRoot
    )

    $profilePath = Join-Path $RepositoryRoot ".github/agents/project.agent.md"
    if (-not (Test-Path -LiteralPath $profilePath -PathType Leaf)) {
        throw "Project profile not found: .github/agents/project.agent.md"
    }

    $profile = Get-Content -LiteralPath $profilePath -Raw
    $match = [regex]::Match($profile, "github_project:\s*.+\(#(?<number>\d+)\)")
    if (-not $match.Success) {
        $match = [regex]::Match($profile, "\(#(?<number>\d+)\)")
    }

    if (-not $match.Success) {
        throw "Could not detect GitHub Project number from .github/agents/project.agent.md. Pass -ProjectNumber."
    }

    return [int]$match.Groups["number"].Value
}

function Get-RepoInfo {
    return Invoke-GhJson -Arguments @("repo", "view", "--json", "nameWithOwner,url")
}

function Get-IssueNumbersFromProjectItems {
    param(
        [Parameter(Mandatory = $true)]
        [object]$ProjectItems,

        [Parameter(Mandatory = $true)]
        [string]$RepositoryNameWithOwner
    )

    $numbers = New-Object System.Collections.Generic.List[int]

    foreach ($item in @($ProjectItems.items)) {
        if ($null -eq $item.content) {
            continue
        }

        if ($item.content.type -ne "Issue") {
            continue
        }

        if ($item.content.repository -and $item.content.repository -ne $RepositoryNameWithOwner) {
            continue
        }

        if ($null -ne $item.content.number) {
            $numbers.Add([int]$item.content.number)
        }
    }

    return @($numbers | Sort-Object -Unique)
}

function Get-IssueDetails {
    param(
        $Numbers,
        $RepositoryNameWithOwner,
        $IncludeClosedIssues
    )

    $issues = @()
    $fields = "number,title,state,url,labels,milestone,assignees,body,comments,createdAt,updatedAt,closedAt,stateReason"

    foreach ($numberValue in $Numbers) {
        $number = [int]$numberValue
        $issue = Invoke-GhJson -Arguments @(
            "issue", "view", ([string]$number),
            "--repo", $RepositoryNameWithOwner,
            "--json", $fields
        )

        if (-not $IncludeClosedIssues -and $issue.state -eq "CLOSED") {
            continue
        }

        $issues += $issue
    }

    return @($issues)
}

function New-MarkdownSnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [object]$RepoInfo,

        [Parameter(Mandatory = $true)]
        [int]$ProjectNumberValue,

        [Parameter(Mandatory = $true)]
        [string]$ProjectOwner,

        [Parameter(Mandatory = $true)]
        [object]$ProjectMetadata,

        [Parameter(Mandatory = $true)]
        [object]$ProjectItems,

        [Parameter(Mandatory = $true)]
        [object[]]$ExportedIssueNumbers,

        [Parameter(Mandatory = $true)]
        [object[]]$Issues,

        [Parameter(Mandatory = $true)]
        [string]$IssueNumberSource,

        [Parameter(Mandatory = $true)]
        [int]$ItemLimit,

        [Parameter(Mandatory = $true)]
        [bool]$ClosedIncluded
    )

    $timestamp = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    $lines = New-Object System.Collections.Generic.List[string]

    $lines.Add("# Raw GitHub Project Snapshot")
    $lines.Add("")
    $lines.Add("[WARNING] This is a temporary raw GitHub Project snapshot for Serena context transfer. It is not curated long-term Serena memory.")
    $lines.Add("[WARNING] Do not treat this file as source code provenance, implementation guidance, or a substitute for repository files and GitHub state.")
    $lines.Add("")
    $lines.Add("Generated at: $timestamp")
    $lines.Add("Repository: $($RepoInfo.nameWithOwner)")
    $lines.Add("Project owner: $ProjectOwner")
    $lines.Add("Project number: $ProjectNumberValue")
    $lines.Add("Project URL: $($ProjectMetadata.url)")
    $lines.Add("Item limit: $ItemLimit")
    $lines.Add("Issue number source: $IssueNumberSource")
    $lines.Add("Include closed issues: $ClosedIncluded")
    $lines.Add("")
    $lines.Add("## Safety Notes")
    $lines.Add("")
    $lines.Add("- This file is generated from GitHub metadata and issue text only.")
    $lines.Add("- It must not contain secrets.")
    $lines.Add("- It must not copy repository source code.")
    $lines.Add("- Refresh it before relying on current planning state.")
    $lines.Add("")
    $lines.Add("## Project Metadata")
    $lines.Add("")
    $lines.Add('```json')
    $lines.Add((ConvertTo-JsonBlock -Value $ProjectMetadata))
    $lines.Add('```')
    $lines.Add("")
    $lines.Add("## Project Items")
    $lines.Add("")
    $lines.Add('```json')
    $lines.Add((ConvertTo-JsonBlock -Value $ProjectItems))
    $lines.Add('```')
    $lines.Add("")
    $lines.Add("## Exported Issue Numbers")
    $lines.Add("")
    if ($ExportedIssueNumbers.Count -eq 0) {
        $lines.Add("No linked repository issues were exported.")
    }
    else {
        $lines.Add(($ExportedIssueNumbers | ForEach-Object { "- #$_" }) -join "`n")
    }
    $lines.Add("")
    $lines.Add("## Linked Issues")
    $lines.Add("")
    $lines.Add('```json')
    $lines.Add((ConvertTo-JsonBlock -Value $Issues))
    $lines.Add('```')
    $lines.Add("")

    return ($lines -join "`n")
}

if ($Limit -lt 1) {
    throw "-Limit must be greater than zero."
}

if (-not (Get-Command "gh" -ErrorAction SilentlyContinue)) {
    throw "GitHub CLI 'gh' was not found on PATH."
}

if (-not (Get-Command "git" -ErrorAction SilentlyContinue)) {
    throw "Git CLI 'git' was not found on PATH."
}

$repoRoot = Resolve-RepositoryRoot
$resolvedOutputPath = Resolve-OutputFile -RepositoryRoot $repoRoot -RequestedPath $OutputPath

Invoke-External -Command "gh" -Arguments @("auth", "status") | Out-Null

$repoInfo = Get-RepoInfo
$repoNameWithOwner = [string]$repoInfo.nameWithOwner

if ([string]::IsNullOrWhiteSpace($Owner)) {
    $Owner = ($repoNameWithOwner -split "/")[0]
}

if (-not $ProjectNumber) {
    $ProjectNumber = Get-ProjectNumberFromProfile -RepositoryRoot $repoRoot
}

$projectMetadata = Invoke-GhJson -Arguments @(
    "project", "view", ([string]$ProjectNumber),
    "--owner", $Owner,
    "--format", "json"
)

$projectItems = Invoke-GhJson -Arguments @(
    "project", "item-list", ([string]$ProjectNumber),
    "--owner", $Owner,
    "--format", "json",
    "--limit", ([string]$Limit)
)

if ($IssueNumbers -and $IssueNumbers.Count -gt 0) {
    $selectedIssueNumbers = @($IssueNumbers | Sort-Object -Unique)
    $issueNumberSource = "manual -IssueNumbers"
}
else {
    $selectedIssueNumbers = @(Get-IssueNumbersFromProjectItems -ProjectItems $projectItems -RepositoryNameWithOwner $repoNameWithOwner)
    $issueNumberSource = "derived from project item content"
}

$selectedIssueNumbers = [int[]]@($selectedIssueNumbers)

$issueDetailParameters = @{
    Numbers = $selectedIssueNumbers
    RepositoryNameWithOwner = $repoNameWithOwner
    IncludeClosedIssues = [bool]$IncludeClosed
}
$issues = @(Get-IssueDetails @issueDetailParameters)

$markdown = New-MarkdownSnapshot `
    -RepoInfo $repoInfo `
    -ProjectNumberValue $ProjectNumber `
    -ProjectOwner $Owner `
    -ProjectMetadata $projectMetadata `
    -ProjectItems $projectItems `
    -ExportedIssueNumbers $selectedIssueNumbers `
    -Issues $issues `
    -IssueNumberSource $issueNumberSource `
    -ItemLimit $Limit `
    -ClosedIncluded ([bool]$IncludeClosed)

if ($DryRun) {
    Write-Host "Dry run: no file was written."
    Write-Host "Repository root: $repoRoot"
    Write-Host "Output path: $resolvedOutputPath"
    Write-Host "Project: $Owner/$ProjectNumber"
    Write-Host "Project items fetched: $($projectItems.items.Count) of $($projectItems.totalCount)"
    Write-Host "Issue numbers exported: $($selectedIssueNumbers -join ', ')"
    Write-Host "Markdown bytes: $([Text.Encoding]::UTF8.GetByteCount($markdown))"
    exit 0
}

$outputDirectory = Split-Path -Parent $resolvedOutputPath
if (-not (Test-Path -LiteralPath $outputDirectory -PathType Container)) {
    New-Item -ItemType Directory -Path $outputDirectory | Out-Null
}

Set-Content -LiteralPath $resolvedOutputPath -Value $markdown -Encoding UTF8

Write-Host "Wrote GitHub Project snapshot: $resolvedOutputPath"
Write-Host "Project: $Owner/$ProjectNumber"
Write-Host "Project items fetched: $($projectItems.items.Count) of $($projectItems.totalCount)"
Write-Host "Issue numbers exported: $($selectedIssueNumbers -join ', ')"
