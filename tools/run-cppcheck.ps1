param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Files
)

$cppcheck = 'C:\Program Files\Cppcheck\cppcheck.exe'
$repoRoot = Split-Path -Parent $PSScriptRoot

if (-not (Test-Path $cppcheck)) {
    Write-Error "cppcheck executable not found at $cppcheck"
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
    --enable=warning,style,performance,portability,information `
    --inline-suppr `
    --language=c++ `
    --std=c++17 `
    -DARDUINO=10819 `
    -DESP32 `
    -DPROGMEM= `
    --quiet `
    @Files

exit $LASTEXITCODE