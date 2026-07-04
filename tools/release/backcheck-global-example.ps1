<#
.SYNOPSIS
Builds a copied example in .Temp as an external PlatformIO consumer.

.DESCRIPTION
Generates a temporary consumer project from a repository example, rewrites
the example's platformio.ini dependency for this library, and compiles it.

By default, the tested package version is read from library.json and resolved
from the PlatformIO registry. Passing -Version forces registry mode for that
exact published version.

.PARAMETER Version
Published package version to test (for example 0.24.0). When provided, the
script always uses the PlatformIO registry package.

.PARAMETER Example
Example path (relative to repository root or absolute path) to copy and build.

.PARAMETER Environment
PlatformIO environment in the generated consumer project.

.PARAMETER PackageOwner
PlatformIO package owner identifier.

.PARAMETER PackageName
PlatformIO package name.

.PARAMETER UseLocalPath
Uses the local repository as symlink dependency instead of registry package.
Cannot be combined with -Version.

.PARAMETER LocalLibraryPath
Local path used when -UseLocalPath is enabled.

.EXAMPLE
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/release/backcheck-global-example.ps1

Uses library.json version from the registry and default example.

.EXAMPLE
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/release/backcheck-global-example.ps1 -Version "0.24.0" -Example "examples/client-object-list-scan-basic"

Forces a published package backcheck for 0.24.0 on the selected example.
#>
[CmdletBinding()]
Param(
  [Alias("PackageVersion")]
  [string] $Version,
  [string] $PackageOwner = "vitaly.ruhl",
  [string] $PackageName = "ESP32 BACnet Stack",
  [string] $Example = "examples/hil-wago-client-acceptance",
  [string] $Environment = "usb",
  [switch] $UseLocalPath,
  [string] $LocalLibraryPath
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
  Param([Parameter(Mandatory = $true)][string] $Path)
  return [System.IO.Path]::GetFullPath($Path)
}

function Get-LibraryVersionFromManifest {
  Param([Parameter(Mandatory = $true)][string] $RepositoryRoot)

  $manifestPath = Join-Path $RepositoryRoot "library.json"
  if (-not (Test-Path -LiteralPath $manifestPath)) {
    throw "library.json not found: $manifestPath"
  }

  $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
  if (-not $manifest.version) {
    throw "library.json does not contain a version field."
  }
  return [string]$manifest.version
}

function Resolve-ExamplePath {
  Param(
    [Parameter(Mandatory = $true)][string] $RepositoryRoot,
    [Parameter(Mandatory = $true)][string] $ExamplePath
  )

  if ([System.IO.Path]::IsPathRooted($ExamplePath)) {
    return Resolve-FullPath $ExamplePath
  }

  return Resolve-FullPath (Join-Path $RepositoryRoot $ExamplePath)
}

function Get-SafeSegment {
  Param([Parameter(Mandatory = $true)][string] $Value)
  return ($Value -replace '[^A-Za-z0-9._-]', '-')
}

function Assert-UnderPath {
  Param(
    [Parameter(Mandatory = $true)][string] $Path,
    [Parameter(Mandatory = $true)][string] $Parent
  )

  $fullPath = Resolve-FullPath $Path
  $fullParent = (Resolve-FullPath $Parent).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
  if (-not $fullPath.StartsWith($fullParent + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to operate outside expected parent: $fullPath is not under $fullParent"
  }
}

function Get-RelativePath {
  Param(
    [Parameter(Mandatory = $true)][string] $BasePath,
    [Parameter(Mandatory = $true)][string] $ChildPath
  )

  $baseFull = (Resolve-FullPath $BasePath).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
  $childFull = Resolve-FullPath $ChildPath
  $baseUri = [System.Uri]::new($baseFull + [System.IO.Path]::DirectorySeparatorChar)
  $childUri = [System.Uri]::new($childFull)
  $relativeUri = $baseUri.MakeRelativeUri($childUri)
  return [System.Uri]::UnescapeDataString($relativeUri.ToString()).Replace('/', [System.IO.Path]::DirectorySeparatorChar)
}

function Copy-ExampleFiltered {
  Param(
    [Parameter(Mandatory = $true)][string] $Source,
    [Parameter(Mandatory = $true)][string] $Destination
  )

  $sourceFull = Resolve-FullPath $Source
  $destinationFull = Resolve-FullPath $Destination

  Get-ChildItem -LiteralPath $sourceFull -Recurse -Force | ForEach-Object {
    $relativePath = Get-RelativePath -BasePath $sourceFull -ChildPath $_.FullName
    $segments = $relativePath -split '[\\/]'

    if ($segments -contains ".pio") {
      return
    }

    if (-not $_.PSIsContainer -and $_.Name -ieq "secrets.h") {
      return
    }

    $targetPath = Join-Path $destinationFull $relativePath
    if ($_.PSIsContainer) {
      New-Item -ItemType Directory -Force -Path $targetPath | Out-Null
      return
    }

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $targetPath) | Out-Null
    Copy-Item -LiteralPath $_.FullName -Destination $targetPath -Force
  }
}

function Assert-NoLocalRepositoryReferences {
  Param(
    [Parameter(Mandatory = $true)][string] $ProjectDir,
    [Parameter(Mandatory = $true)][string] $RepositoryRoot
  )

  $platformioIni = Join-Path $ProjectDir "platformio.ini"
  $content = Get-Content -Raw -LiteralPath $platformioIni
  $forbiddenPatterns = @(
    "lib_extra_dirs\s*=",
    "symlink://",
    "file://",
    "\.\.[\\/]",
    [regex]::Escape((Resolve-FullPath $RepositoryRoot))
  )

  foreach ($pattern in $forbiddenPatterns) {
    if ($content -match $pattern) {
      throw "Forbidden local repository reference remains in platformio.ini: $pattern"
    }
  }

  Get-ChildItem -LiteralPath $ProjectDir -Recurse -Force | ForEach-Object {
    if (($_.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
      $resolved = Resolve-Path -LiteralPath $_.FullName
      foreach ($resolvedPath in $resolved) {
        $fullResolvedPath = Resolve-FullPath $resolvedPath.Path
        $repoRootFull = (Resolve-FullPath $RepositoryRoot).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
        $projectFull = (Resolve-FullPath $ProjectDir).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
        if ($fullResolvedPath.StartsWith($repoRootFull + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) -and
            -not $fullResolvedPath.StartsWith($projectFull + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
          throw "Project contains a reparse point into the repository: $($_.FullName) -> $fullResolvedPath"
        }
      }
    }
  }
}

function Update-PlatformioDependency {
  Param(
    [Parameter(Mandatory = $true)][string] $PlatformioIniPath,
    [Parameter(Mandatory = $true)][string] $Environment,
    [Parameter(Mandatory = $true)][string] $DependencyLine,
    [Parameter(Mandatory = $true)][string] $PackageName,
    [Parameter(Mandatory = $true)][string] $PackageOwner,
    [Parameter(Mandatory = $true)][string] $RepositoryRoot,
    [Parameter(Mandatory = $true)][bool] $PublishedMode
  )

  $lines = [System.Collections.Generic.List[string]]::new()
  $lines.AddRange([string[]](Get-Content -LiteralPath $PlatformioIniPath))

  $targetEnvHeader = "[env:$Environment]"
  $currentEnv = ""
  $inTargetEnv = $false
  $inLibDeps = $false
  $foundTargetEnv = $false
  $insertedDependency = $false

  $repoRootPattern = [regex]::Escape((Resolve-FullPath $RepositoryRoot))
  $packagePattern = "(?i)($([regex]::Escape($PackageOwner))\/$([regex]::Escape($PackageName))|$([regex]::Escape($PackageName)))"
  $localPattern = "(?i)(symlink://|file://|$repoRootPattern)"

  $output = [System.Collections.Generic.List[string]]::new()

  for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]

    if ($line -match '^\s*\[(.+)\]\s*$') {
      if ($inTargetEnv -and $inLibDeps -and -not $insertedDependency) {
        $output.Add("  $DependencyLine")
        $insertedDependency = $true
      }
      $inLibDeps = $false
      $currentEnv = $Matches[1]
      $inTargetEnv = ("[$currentEnv]" -ieq $targetEnvHeader)
      if ($inTargetEnv) {
        $foundTargetEnv = $true
      }
      $output.Add($line)
      continue
    }

    if ($inTargetEnv -and $line -match '^\s*lib_deps\s*=\s*$') {
      $inLibDeps = $true
      $output.Add($line)
      continue
    }

    if ($inTargetEnv -and $inLibDeps) {
      if ($line -match '^\s*$') {
        if (-not $insertedDependency) {
          $output.Add("  $DependencyLine")
          $insertedDependency = $true
        }
        $output.Add($line)
        $inLibDeps = $false
        continue
      }

      if ($line -match '^\s*[A-Za-z0-9_.-]+\s*=.*$') {
        if (-not $insertedDependency) {
          $output.Add("  $DependencyLine")
          $insertedDependency = $true
        }
        $output.Add($line)
        $inLibDeps = $false
        continue
      }

      $isPackageLine = $line -match $packagePattern
      $isLocalLine = $line -match $localPattern

      if ($isPackageLine -or $isLocalLine) {
        continue
      }

      $output.Add($line)
      continue
    }

    $output.Add($line)
  }

  if (-not $foundTargetEnv) {
    throw "Environment section not found in platformio.ini: $targetEnvHeader"
  }

  if ($inTargetEnv -and $inLibDeps -and -not $insertedDependency) {
    $output.Add("  $DependencyLine")
    $insertedDependency = $true
  }

  if (-not $insertedDependency) {
    for ($i = 0; $i -lt $output.Count; $i++) {
      if ($output[$i] -ieq $targetEnvHeader) {
        $insertAt = $i + 1
        while ($insertAt -lt $output.Count -and $output[$insertAt] -notmatch '^\s*\[') {
          $insertAt++
        }
        $output.Insert($insertAt, "")
        $output.Insert($insertAt + 1, "lib_deps =")
        $output.Insert($insertAt + 2, "  $DependencyLine")
        $insertedDependency = $true
        break
      }
    }
  }

  if (-not $insertedDependency) {
    throw "Unable to inject dependency into platformio.ini for $targetEnvHeader"
  }

  Set-Utf8NoBomContent -Path $PlatformioIniPath -Value (($output -join "`n") + "`n")

  if ($PublishedMode) {
    Assert-NoLocalRepositoryReferences -ProjectDir (Split-Path -Parent $PlatformioIniPath) -RepositoryRoot $RepositoryRoot
  }
}

function Invoke-CheckedCommand {
  Param(
    [Parameter(Mandatory = $true)][string] $Command,
    [Parameter(Mandatory = $true)][string[]] $Arguments,
    [Parameter(Mandatory = $true)][string] $WorkingDirectory
  )

  $display = "$Command $($Arguments -join ' ')"
  Write-Host ""
  Write-Host ">>> $display"

  Push-Location $WorkingDirectory
  try {
    $output = @()
    & $Command @Arguments 2>&1 | ForEach-Object {
      $line = $_
      $output += $line
      Write-Host $line
    }
    $exitCode = $LASTEXITCODE
  } finally {
    Pop-Location
  }

  $text = ($output | Out-String)
  if ($exitCode -ne 0) {
    throw "Command failed with exit code ${exitCode}: $display`n$text"
  }

  return $text
}

function Set-Utf8NoBomContent {
  Param(
    [Parameter(Mandatory = $true)][string] $Path,
    [Parameter(Mandatory = $true)][string] $Value
  )

  $encoding = New-Object System.Text.UTF8Encoding $false
  [System.IO.File]::WriteAllText((Resolve-FullPath $Path), $Value, $encoding)
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-FullPath (Join-Path $scriptDir "..\..")

if ($UseLocalPath -and $PSBoundParameters.ContainsKey("Version")) {
  throw "-Version forces published registry mode and cannot be combined with -UseLocalPath."
}

$effectiveVersion = $Version
if (-not $effectiveVersion) {
  $effectiveVersion = Get-LibraryVersionFromManifest -RepositoryRoot $repoRoot
}

$publishedMode = -not $UseLocalPath
$modeLabel = if ($publishedMode) { "published-registry" } else { "local-path" }

$sourceExample = Resolve-ExamplePath -RepositoryRoot $repoRoot -ExamplePath $Example
$exampleName = Split-Path -Leaf $sourceExample
$backcheckRoot = Resolve-FullPath (Join-Path $repoRoot ".Temp\release-backcheck")
$versionTag = Get-SafeSegment $effectiveVersion
$modeTag = Get-SafeSegment $modeLabel
$consumerProject = Join-Path $backcheckRoot ("{0}-{1}-{2}-{3}" -f $exampleName, $Environment, $modeTag, $versionTag)

$resolvedLocalLibraryPath = $null
if ($UseLocalPath) {
  if ($LocalLibraryPath) {
    $resolvedLocalLibraryPath = Resolve-FullPath $LocalLibraryPath
  } else {
    $resolvedLocalLibraryPath = $repoRoot
  }
  Assert-UnderPath -Path $resolvedLocalLibraryPath -Parent $repoRoot
}

$libSpec = if ($publishedMode) {
  "${PackageOwner}/${PackageName}@${effectiveVersion}"
} else {
  "symlink://$resolvedLocalLibraryPath"
}

if (-not (Test-Path -LiteralPath $sourceExample)) {
  throw "Source example not found: $sourceExample"
}

New-Item -ItemType Directory -Force -Path $backcheckRoot | Out-Null
Assert-UnderPath -Path $consumerProject -Parent $backcheckRoot

if (Test-Path -LiteralPath $consumerProject) {
  Remove-Item -LiteralPath $consumerProject -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $consumerProject | Out-Null
Copy-ExampleFiltered -Source $sourceExample -Destination $consumerProject

$consumerPlatformioIni = Join-Path $consumerProject "platformio.ini"
if (-not (Test-Path -LiteralPath $consumerPlatformioIni)) {
  throw "Copied example is missing platformio.ini: $consumerPlatformioIni"
}

Update-PlatformioDependency `
  -PlatformioIniPath $consumerPlatformioIni `
  -Environment $Environment `
  -DependencyLine $libSpec `
  -PackageName $PackageName `
  -PackageOwner $PackageOwner `
  -RepositoryRoot $repoRoot `
  -PublishedMode $publishedMode

$secretsExample = Join-Path $consumerProject "src\secret\secrets.example.h"
$dummySecrets = Join-Path $consumerProject "src\secret\secrets.h"
if (Test-Path -LiteralPath $secretsExample) {
  $dummyContent = @(
    "// Generated by tools/release/backcheck-global-example.ps1 for compile-only release backcheck."
    "// Contains placeholder values copied from secrets.example.h; do not add real credentials here."
    ""
    (Get-Content -Raw -LiteralPath $secretsExample)
  ) -join "`n"
  Set-Utf8NoBomContent -Path $dummySecrets -Value $dummyContent
}

Write-Host ""
Write-Host "Release backcheck configuration"
Write-Host "  Source example path : $sourceExample"
Write-Host "  Temp project path   : $consumerProject"
Write-Host "  Environment         : $Environment"
Write-Host "  Mode                : $modeLabel"
Write-Host "  Version tested      : $effectiveVersion"
Write-Host "  Dependency used     : $libSpec"

Invoke-CheckedCommand -Command "pio" -Arguments @("pkg", "install") -WorkingDirectory $consumerProject | Out-Null

if ($publishedMode) {
  $libDepsDir = Join-Path $consumerProject (".pio\libdeps\{0}" -f $Environment)
  if (-not (Test-Path -LiteralPath $libDepsDir)) {
    throw "PlatformIO libdeps directory not found after install: $libDepsDir"
  }

  $installedManifest = Get-ChildItem -LiteralPath $libDepsDir -Recurse -Force -Filter "library.json" |
    Where-Object {
      $manifest = Get-Content -Raw -LiteralPath $_.FullName | ConvertFrom-Json
      $manifest.name -eq $PackageName
    } |
    Select-Object -First 1

  if (-not $installedManifest) {
    throw "Installed PlatformIO package manifest for '$PackageName' not found under $libDepsDir"
  }

  $installed = Get-Content -Raw -LiteralPath $installedManifest.FullName | ConvertFrom-Json
  if ($installed.version -ne $effectiveVersion) {
    throw "Installed package version mismatch: expected $effectiveVersion, got $($installed.version)"
  }

  $installedPath = Resolve-FullPath (Split-Path -Parent $installedManifest.FullName)
  $repoRootFull = (Resolve-FullPath $repoRoot).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
  $consumerFull = (Resolve-FullPath $consumerProject).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
  if ($installedPath.StartsWith($repoRootFull + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) -and
      -not $installedPath.StartsWith($consumerFull + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Installed package resolves to repository source path: $installedPath"
  }

  Write-Host "Installed package manifest: $($installedManifest.FullName)"
  Write-Host "Installed package version : $($installed.version)"
  Write-Host "Source isolation check   : passed"
}

Invoke-CheckedCommand -Command "pio" -Arguments @("run", "-e", $Environment) -WorkingDirectory $consumerProject | Out-Null

Write-Host ""
Write-Host "Release backcheck passed."
Write-Host "  Built dependency : $libSpec"
Write-Host "  Consumer project : $consumerProject"
Write-Host "  Environment      : $Environment"
Write-Host "  Mode             : $modeLabel"
Write-Host "  Version tested   : $effectiveVersion"

exit 0
