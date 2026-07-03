Param(
  [string] $PackageVersion = "0.18.0",
  [string] $PackageOwner = "vitaly.ruhl",
  [string] $PackageName = "ESP32 BACnet Stack"
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
  Param([Parameter(Mandatory = $true)][string] $Path)
  return [System.IO.Path]::GetFullPath($Path)
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
$sourceExample = Join-Path $repoRoot "examples\hil-wago-client-acceptance"
$backcheckRoot = Join-Path $repoRoot ".Temp\release-backcheck"
$consumerProject = Join-Path $backcheckRoot "hil-wago-client-acceptance-global"
$libSpec = "${PackageOwner}/${PackageName}@${PackageVersion}"

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

$platformioIni = @"
; PlatformIO Project Configuration File
;
; Generated release backcheck consumer project. Do not use local repository sources.

[platformio]
description = External WAGO BACnet/IP client acceptance release backcheck

[env:usb]
platform = espressif32
board = nodemcu-32s
framework = arduino
monitor_speed = 115200

lib_deps =
  $libSpec

build_unflags =
  -std=gnu++11

build_flags =
  -Wno-deprecated-declarations
  -std=gnu++17
"@
Set-Utf8NoBomContent -Path (Join-Path $consumerProject "platformio.ini") -Value $platformioIni

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

Assert-NoLocalRepositoryReferences -ProjectDir $consumerProject -RepositoryRoot $repoRoot

Write-Host "Release backcheck consumer project: $consumerProject"
Write-Host "Package dependency: $libSpec"

Invoke-CheckedCommand -Command "pio" -Arguments @("pkg", "install") -WorkingDirectory $consumerProject | Out-Null

$libDepsDir = Join-Path $consumerProject ".pio\libdeps\usb"
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
if ($installed.version -ne $PackageVersion) {
  throw "Installed package version mismatch: expected $PackageVersion, got $($installed.version)"
}

$installedPath = Resolve-FullPath (Split-Path -Parent $installedManifest.FullName)
$repoRootFull = (Resolve-FullPath $repoRoot).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
$consumerFull = (Resolve-FullPath $consumerProject).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
if ($installedPath.StartsWith($repoRootFull + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) -and
    -not $installedPath.StartsWith($consumerFull + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
  throw "Installed package resolves to repository source path: $installedPath"
}

Write-Host "Installed package manifest: $($installedManifest.FullName)"
Write-Host "Installed package version: $($installed.version)"
Write-Host "Local repository source check: passed"

$runOutput = Invoke-CheckedCommand -Command "pio" -Arguments @("run") -WorkingDirectory $consumerProject

Write-Host ""
Write-Host "Release backcheck passed."
Write-Host "Built package: $libSpec"
Write-Host "Consumer project: $consumerProject"

exit 0
