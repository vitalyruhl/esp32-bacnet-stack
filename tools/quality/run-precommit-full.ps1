param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments
)

# The canonical repository-wide entry point is
# tools/build/run-full-repository-gate.ps1. This runner remains the narrow
# autofix-capable pre-commit and cppcheck component used by that gate.

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\\..")).Path

Push-Location $repoRoot
try {
    Write-Host 'Running full pre-commit check on the repository'
    & pre-commit run --all-files @Arguments
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    Write-Host 'Running mandatory cppcheck governance gate'
    & pwsh -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'run-cppcheck.ps1')
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
