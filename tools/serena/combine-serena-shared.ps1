$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")
$repoRootPath = $repoRoot.Path

$memoryDir = Join-Path $repoRootPath ".serena\memories\shared"
$outDir = Join-Path $repoRootPath ".Temp"
$out = Join-Path $outDir "serena-memory.md"

if (-not (Test-Path -LiteralPath $memoryDir)) {
    throw "Serena shared memory directory not found: $memoryDir"
}

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

if (Test-Path -LiteralPath $out) {
    Remove-Item -LiteralPath $out
}

$files = @(
    Get-ChildItem -LiteralPath $memoryDir -Filter "*.md" -File |
        Sort-Object Name
)

if ($files.Count -eq 0) {
    throw "No shared Serena memory markdown files found in: $memoryDir"
}

$files |
    ForEach-Object {
        $relativePath = [System.IO.Path]::GetRelativePath($repoRootPath, $_.FullName)

        "# FILE: $relativePath"
        ""
        Get-Content -LiteralPath $_.FullName -Raw
        ""
        "---"
        ""
    } |
    Set-Content -LiteralPath $out -Encoding UTF8

Get-Item -LiteralPath $out | Select-Object FullName, Length, LastWriteTime
