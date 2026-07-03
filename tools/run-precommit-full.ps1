param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments
)

$repoRoot = Split-Path -Parent $PSScriptRoot

Push-Location $repoRoot
try {
    Write-Host 'Running full pre-commit check on the repository'
    & pre-commit run --all-files @Arguments
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}