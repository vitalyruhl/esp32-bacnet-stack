param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Files
)

$cppcheck = 'C:\Program Files\Cppcheck\cppcheck.exe'

if (-not (Test-Path $cppcheck)) {
    Write-Error "cppcheck executable not found at $cppcheck"
    exit 1
}

if (-not $Files -or $Files.Count -eq 0) {
    exit 0
}

& $cppcheck `
    --enable=warning,style,performance,portability,information `
    --inline-suppr `
    --language=c++ `
    --std=c++17 `
    --quiet `
    @Files

exit $LASTEXITCODE