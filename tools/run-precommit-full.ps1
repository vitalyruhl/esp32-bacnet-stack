param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments
)

$repoRoot = Split-Path -Parent $PSScriptRoot

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
