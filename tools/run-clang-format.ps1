param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Files
)

$clangFormatCandidates = @(
    'C:\Program Files\LLVM\bin\clang-format.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\bin\clang-format.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-format.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\ARM64\bin\clang-format.exe'
)

$clangFormat = $clangFormatCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $clangFormat) {
    Write-Error 'clang-format executable not found.'
    exit 1
}

$repoRoot = Split-Path -Parent $PSScriptRoot

function Get-FormatTargets {
    param(
        [string[]]$InputPaths
    )

    $extensions = @('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.ino')
    $resolved = @()

    foreach ($inputPath in $InputPaths) {
        if (Test-Path $inputPath -PathType Container) {
            $resolved += Get-ChildItem -Path $inputPath -Recurse -File |
                Where-Object { $extensions -contains $_.Extension.ToLowerInvariant() } |
                ForEach-Object { $_.FullName }
            continue
        }

        if (Test-Path $inputPath -PathType Leaf) {
            $resolved += (Resolve-Path $inputPath).Path
        }
    }

    return $resolved
}

if (-not $Files -or $Files.Count -eq 0) {
    $Files = @(
        (Join-Path $repoRoot 'src'),
        (Join-Path $repoRoot 'examples'),
        (Join-Path $repoRoot 'test')
    )
}

$Files = Get-FormatTargets -InputPaths $Files

if (-not $Files -or $Files.Count -eq 0) {
    Write-Error 'No clang-format targets found.'
    exit 1
}

Write-Host "Running clang-format on: $($Files -join ', ')"

& $clangFormat `
    --dry-run `
    --Werror `
    --style=file `
    @Files

exit $LASTEXITCODE