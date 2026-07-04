<#
.SYNOPSIS
Builds a copied example in .Temp as an external PlatformIO consumer.

.DESCRIPTION
Generates a temporary consumer project from a repository example, rewrites
the example's platformio.ini dependency for this library, and compiles it.

Optional HIL flags can upload the generated consumer project to ESP32 hardware
and open a serial monitor.

By default, the tested package version is read from library.json and resolved
from the PlatformIO registry. Passing -Version forces registry mode for that
exact published version.

.PARAMETER Version
Published package version to test (for example 0.24.2). When provided, the
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

.PARAMETER Upload
Uploads the generated consumer project after a successful build.

.PARAMETER Monitor
Starts PlatformIO serial monitor for the generated consumer project.

.PARAMETER UploadPort
Upload port passed to PlatformIO when -Upload is used.

.PARAMETER MonitorPort
Monitor port passed to PlatformIO when -Monitor is used.

.PARAMETER MonitorBaud
Serial monitor baud rate when -Monitor is used. Default: 115200.

.EXAMPLE
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/release/backcheck-global-example.ps1

Uses library.json version from the registry and default example.

.EXAMPLE
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/release/backcheck-global-example.ps1 -Version "0.24.2" -Example "examples/client-object-list-scan-basic"

Forces a published package backcheck for 0.24.2 on the selected example.

.EXAMPLE
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/release/backcheck-global-example.ps1 -Version "0.24.2"

Compile-only published package check.

.EXAMPLE
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/release/backcheck-global-example.ps1 -Version "0.24.2" -Example "examples/hil-wago-client-acceptance" -Upload -UploadPort "COM5"

Published package HIL upload.

.EXAMPLE
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/release/backcheck-global-example.ps1 -Version "0.24.2" -Example "examples/hil-wago-client-acceptance" -Upload -Monitor -UploadPort "COM5" -MonitorBaud 115200

Published package HIL upload and monitor.
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
  [string] $LocalLibraryPath,
  [switch] $Upload,
  [switch] $Monitor,
  [string] $UploadPort,
  [string] $MonitorPort,
  [int] $MonitorBaud = 115200
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

function Invoke-InteractiveCommand {
  Param(
    [Parameter(Mandatory = $true)][string] $Command,
    [Parameter(Mandatory = $true)][string[]] $Arguments,
    [Parameter(Mandatory = $true)][string] $WorkingDirectory,
    [long[]] $AcceptedExitCodes = @(0)
  )

  $display = "$Command $($Arguments -join ' ')"
  Write-Host ""
  Write-Host ">>> $display"

  Push-Location $WorkingDirectory
  try {
    & $Command @Arguments
    $exitCode = $LASTEXITCODE
  } finally {
    Pop-Location
  }

  foreach ($accepted in $AcceptedExitCodes) {
    if ($exitCode -eq $accepted) {
      return
    }
  }

  throw "Command failed with exit code ${exitCode}: $display"
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

if ($UploadPort -and -not $Upload) {
  throw "-UploadPort requires -Upload."
}

if ($MonitorBaud -le 0) {
  throw "-MonitorBaud must be greater than zero."
}

$effectiveVersion = $Version
if (-not $effectiveVersion) {
  $effectiveVersion = Get-LibraryVersionFromManifest -RepositoryRoot $repoRoot
}

$publishedMode = -not $UseLocalPath
$modeLabel = if ($publishedMode) { "published-registry" } else { "local-path" }
$effectiveMonitorPort = $MonitorPort
if ($Monitor -and -not $effectiveMonitorPort -and $UploadPort) {
  $effectiveMonitorPort = $UploadPort
}

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

$sourceLocalSecrets = Join-Path $sourceExample "src\secret\secrets.h"
$secretsExample = Join-Path $consumerProject "src\secret\secrets.example.h"
$consumerSecrets = Join-Path $consumerProject "src\secret\secrets.h"
$secretsMode = "none"

if (($Upload -or $Monitor) -and (Test-Path -LiteralPath $sourceLocalSecrets)) {
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $consumerSecrets) | Out-Null
  Copy-Item -LiteralPath $sourceLocalSecrets -Destination $consumerSecrets -Force
  $secretsMode = "local-secrets-copied"
} elseif (Test-Path -LiteralPath $secretsExample) {
  $dummyContent = @(
    "// Generated by tools/release/backcheck-global-example.ps1 for compile-only release backcheck."
    "// Contains placeholder values copied from secrets.example.h; do not add real credentials here."
    ""
    (Get-Content -Raw -LiteralPath $secretsExample)
  ) -join "`n"
  Set-Utf8NoBomContent -Path $consumerSecrets -Value $dummyContent
  $secretsMode = "placeholder-from-example"
}

if (($Upload -or $Monitor) -and $secretsMode -ne "local-secrets-copied") {
  Write-Host "[W] HIL mode requested, but no local secrets.h found in source example."
}

Write-Host ""
Write-Host "Release backcheck configuration"
Write-Host "  Source example path : $sourceExample"
Write-Host "  Temp project path   : $consumerProject"
Write-Host "  Environment         : $Environment"
Write-Host "  Mode                : $modeLabel"
Write-Host "  Version tested      : $effectiveVersion"
Write-Host "  Dependency used     : $libSpec"
Write-Host "  Secrets mode        : $secretsMode"
Write-Host "  Upload enabled      : $($Upload.IsPresent)"
if ($Upload) {
  Write-Host "  Upload port         : $(if ($UploadPort) { $UploadPort } else { '<default>' })"
}
Write-Host "  Monitor enabled     : $($Monitor.IsPresent)"
if ($Monitor) {
  Write-Host "  Monitor port        : $(if ($effectiveMonitorPort) { $effectiveMonitorPort } else { '<default>' })"
  Write-Host "  Monitor baud        : $MonitorBaud"
}

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

$uploadPassed = $false
if ($Upload) {
  $uploadArgs = @("run", "-e", $Environment, "-t", "upload")
  if ($UploadPort) {
    $uploadArgs += @("--upload-port", $UploadPort)
  }
  Invoke-CheckedCommand -Command "pio" -Arguments $uploadArgs -WorkingDirectory $consumerProject | Out-Null
  $uploadPassed = $true
}

$monitorStarted = $false
if ($Monitor) {
  Write-Host ""
  Write-Host "Monitor is interactive. Stop it with Ctrl+C when you are done."

  $monitorArgs = @("device", "monitor", "-e", $Environment, "--baud", "$MonitorBaud")
  if ($effectiveMonitorPort) {
    $monitorArgs += @("--port", $effectiveMonitorPort)
  }

  $monitorStarted = $true
  Invoke-InteractiveCommand `
    -Command "pio" `
    -Arguments $monitorArgs `
    -WorkingDirectory $consumerProject `
    -AcceptedExitCodes @(0, 1, 130, -1073741510)
}

Write-Host ""
Write-Host "Release backcheck passed."
Write-Host "  Build            : passed"
if ($Upload) {
  Write-Host "  Upload           : $(if ($uploadPassed) { 'passed' } else { 'not run' })"
} else {
  Write-Host "  Upload           : skipped (not requested)"
}
if ($Monitor) {
  Write-Host "  Monitor          : $(if ($monitorStarted) { 'started (interactive)' } else { 'not started' })"
} else {
  Write-Host "  Monitor          : skipped (not requested)"
}
Write-Host "  Built dependency : $libSpec"
Write-Host "  Consumer project : $consumerProject"
Write-Host "  Environment      : $Environment"
Write-Host "  Mode             : $modeLabel"
Write-Host "  Version tested   : $effectiveVersion"

exit 0
