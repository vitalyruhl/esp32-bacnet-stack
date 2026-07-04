param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Files
)

$repoRoot = Split-Path -Parent $PSScriptRoot

$cppcheckCandidates = @(
    'C:\Program Files\Cppcheck\cppcheck.exe',
    'cppcheck'
)

$cppcheck = $null
foreach ($candidate in $cppcheckCandidates) {
    if (Test-Path -LiteralPath $candidate) {
        $cppcheck = $candidate
        break
    }

    $resolvedCommand = Get-Command $candidate -ErrorAction SilentlyContinue
    if ($resolvedCommand) {
        $cppcheck = $resolvedCommand.Source
        break
    }
}

if (-not (Test-Path $cppcheck)) {
    Write-Error "cppcheck executable not found. Install cppcheck or add it to PATH."
    exit 1
}

if (-not $Files -or $Files.Count -eq 0) {
    $Files = @(
        (Join-Path $repoRoot 'src'),
        (Join-Path $repoRoot 'examples'),
        (Join-Path $repoRoot 'test')
    )
}

Write-Host "Running cppcheck on: $($Files -join ', ')"

& $cppcheck `
    --error-exitcode=1 `
    --enable=warning,style,performance,portability `
    --inline-suppr `
    --language=c++ `
    --std=c++17 `
    -DARDUINO=10819 `
    -DESP32 `
    -DPROGMEM= `
    --quiet `
    @Files

exit $LASTEXITCODE
