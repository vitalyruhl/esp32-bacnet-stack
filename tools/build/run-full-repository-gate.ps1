<#
.SYNOPSIS
Runs the reproducible full repository validation gate.

.DESCRIPTION
Creates a pristine Git archive below C:\Temp and validates only that versioned
state. Ignored local files, including local secrets, are never copied into the
gate workspace. Every check writes a dedicated log and the run writes a JSON
summary.

.PARAMETER FailFast
Stops after the first unexpected failed or skipped check.

.PARAMETER KeepLogs
Retains the pristine gate workspace and consumer project in addition to the
always-retained logs and JSON summary.

.PARAMETER Clean
Removes prior gate run directories below the dedicated C:\Temp root before the
new run. The current run is never removed.

.PARAMETER OnlyFailedFrom
Repeats only checks whose status was FAIL in a prior gate JSON summary. Archive
creation and PlatformIO discovery still run as required setup.

.PARAMETER DiscoverOnly
Validates archive creation and PlatformIO discovery without compiling targets.
#>
[CmdletBinding()]
param(
    [switch]$FailFast,
    [switch]$KeepLogs,
    [switch]$Clean,
    [string]$OnlyFailedFrom,
    [switch]$DiscoverOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:Results = [System.Collections.Generic.List[object]]::new()
$script:FailedNames = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::Ordinal
)
$script:OnlyFailedNames = $null
$script:LastNativeExitCode = 0
$script:PlatformIoMatrix = @()
$script:PlatformIoTestTargets = @()
$script:StopRequested = $false

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$tempRoot = 'C:\Temp\esp32-bacnet-stack-full-gate'
$runStarted = Get-Date

function ConvertTo-CommandText {
    param([string]$File, [string[]]$Arguments)

    $quotedArguments = $Arguments | ForEach-Object {
        if ($_ -match '[\s"]') {
            '"{0}"' -f $_.Replace('"', '\"')
        }
        else {
            $_
        }
    }
    return (@($File) + $quotedArguments) -join ' '
}

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory)][string]$File,
        [string[]]$Arguments = @(),
        [string]$WorkingDirectory = $repoRoot
    )

    Push-Location -LiteralPath $WorkingDirectory
    try {
        & $File @Arguments
        $script:LastNativeExitCode = $LASTEXITCODE
        if ($LASTEXITCODE -ne 0) {
            throw "Command exited with code ${LASTEXITCODE}: $(ConvertTo-CommandText -File $File -Arguments $Arguments)"
        }
    }
    finally {
        Pop-Location
    }
}

function Add-Result {
    param(
        [string]$Name,
        [ValidateSet('PASS', 'FAIL', 'SKIP')][string]$Status,
        [string]$Command,
        [int]$ExitCode,
        [string]$LogPath,
        [TimeSpan]$Duration,
        [bool]$ExpectedFailure = $false,
        [string]$Detail = ''
    )

    $script:Results.Add([pscustomobject]([ordered]@{
                name = $Name
                status = $Status
                expected_failure = $ExpectedFailure
                command = $Command
                exit_code = $ExitCode
                duration_ms = [Math]::Round($Duration.TotalMilliseconds)
                log_path = $LogPath
                detail = $Detail
            }))
}

function Show-FailureExcerpt {
    param([string]$LogPath)

    Write-Host "  log: $LogPath"
    if (Test-Path -LiteralPath $LogPath) {
        Get-Content -LiteralPath $LogPath -Tail 18 | ForEach-Object {
            Write-Host "  $_"
        }
    }
}

function Should-RunCheck {
    param([string]$Name)

    if ($null -eq $script:OnlyFailedNames) {
        return $true
    }
    return $script:OnlyFailedNames.Contains($Name)
}

function Invoke-GateCheck {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$Command,
        [Parameter(Mandatory)][scriptblock]$Action,
        [switch]$ExpectedFailure,
        [switch]$RequiredSetup
    )

    if ($script:StopRequested) {
        return
    }
    if (-not $RequiredSetup -and -not (Should-RunCheck -Name $Name)) {
        return
    }

    $safeName = ($Name -replace '[^A-Za-z0-9._-]', '_')
    $logPath = Join-Path $script:LogsDirectory ("$safeName.log")
    Set-Content -LiteralPath $logPath -Value @(
        "Name: $Name"
        "Command: $Command"
        "Started: $([DateTime]::UtcNow.ToString('o'))"
        ''
    ) -Encoding utf8

    Write-Host "[RUN] $Name"
    $script:LastNativeExitCode = 0
    $stopwatch = [Diagnostics.Stopwatch]::StartNew()
    $succeeded = $false
    $detail = ''
    try {
        & $Action *>> $logPath
        $succeeded = $true
    }
    catch {
        $detail = $_.Exception.Message
        Add-Content -LiteralPath $logPath -Value "`nException: $detail" -Encoding utf8
    }
    finally {
        $stopwatch.Stop()
    }

    # A failed-only rerun still needs a clean archive and fresh discovery, but
    # those setup steps are not repeated validation checks in its JSON report.
    if ($RequiredSetup -and $null -ne $script:OnlyFailedNames -and $succeeded) {
        Write-Host "[PASS] $Name (setup)"
        return
    }

    if ($ExpectedFailure) {
        if (-not $succeeded) {
            Add-Result -Name $Name -Status PASS -Command $Command -ExitCode $script:LastNativeExitCode -LogPath $logPath -Duration $stopwatch.Elapsed -ExpectedFailure $true -Detail 'Expected compile failure observed.'
            Write-Host "[PASS] $Name (expected failure)"
            return
        }

        $detail = 'Expected compile failure unexpectedly succeeded.'
        Add-Result -Name $Name -Status FAIL -Command $Command -ExitCode 0 -LogPath $logPath -Duration $stopwatch.Elapsed -ExpectedFailure $true -Detail $detail
        $script:FailedNames.Add($Name) | Out-Null
        Write-Host "[FAIL] $Name"
        Write-Host "  command: $Command"
        Write-Host '  exitcode: 0'
        Show-FailureExcerpt -LogPath $logPath
    }
    elseif ($succeeded) {
        Add-Result -Name $Name -Status PASS -Command $Command -ExitCode 0 -LogPath $logPath -Duration $stopwatch.Elapsed -Detail ''
        Write-Host "[PASS] $Name"
        return
    }
    else {
        $exitCode = if ($script:LastNativeExitCode -ne 0) { $script:LastNativeExitCode } else { 1 }
        Add-Result -Name $Name -Status FAIL -Command $Command -ExitCode $exitCode -LogPath $logPath -Duration $stopwatch.Elapsed -Detail $detail
        $script:FailedNames.Add($Name) | Out-Null
        Write-Host "[FAIL] $Name"
        Write-Host "  command: $Command"
        Write-Host "  exitcode: $exitCode"
        Show-FailureExcerpt -LogPath $logPath
    }

    if ($FailFast) {
        $script:StopRequested = $true
    }
}

function Skip-GateCheck {
    param([string]$Name, [string]$Reason)

    if ($script:StopRequested -or -not (Should-RunCheck -Name $Name)) {
        return
    }

    $logPath = Join-Path $script:LogsDirectory ((($Name -replace '[^A-Za-z0-9._-]', '_')) + '.log')
    Set-Content -LiteralPath $logPath -Value "SKIP: $Reason" -Encoding utf8
    Add-Result -Name $Name -Status SKIP -Command '' -ExitCode 0 -LogPath $logPath -Duration [TimeSpan]::Zero -Detail $Reason
    $script:FailedNames.Add($Name) | Out-Null
    Write-Host "[SKIP] $Name"
    Write-Host "  reason: $Reason"
    if ($FailFast) {
        $script:StopRequested = $true
    }
}

function Get-PlatformIoMatrix {
    param([string]$GateRoot)

    $matrix = [System.Collections.Generic.List[object]]::new()
    $testTargets = [System.Collections.Generic.List[object]]::new()
    $iniFiles = Get-ChildItem -LiteralPath $GateRoot -Filter 'platformio.ini' -File -Recurse | Sort-Object FullName
    foreach ($iniFile in $iniFiles) {
        $projectDirectory = $iniFile.DirectoryName
        $relativePath = [IO.Path]::GetRelativePath($GateRoot, $projectDirectory).Replace('\', '/')
        if ([string]::IsNullOrWhiteSpace($relativePath) -or $relativePath -eq '.') {
            $relativePath = '.'
        }

        $configOutput = & pio project config --json-output -d $projectDirectory
        $script:LastNativeExitCode = $LASTEXITCODE
        if ($LASTEXITCODE -ne 0) {
            throw "PlatformIO discovery failed for $relativePath with exit code $LASTEXITCODE."
        }
        $config = $configOutput | ConvertFrom-Json
        $environments = @($config | Where-Object { $_[0] -like 'env:*' } | ForEach-Object { [string]$_[0].Substring(4) })
        if ($environments.Count -eq 0) {
            throw "No PlatformIO environments found in $relativePath."
        }

        foreach ($environment in $environments) {
            $matrix.Add([pscustomobject]@{
                    project = $relativePath
                    directory = $projectDirectory
                    environment = $environment
                })
        }

        $platformEntry = $config | Where-Object { $_[0] -eq 'platformio' } | Select-Object -First 1
        $testDirectory = $null
        if ($platformEntry) {
            $testEntry = $platformEntry[1] | Where-Object { $_[0] -eq 'test_dir' } | Select-Object -First 1
            if ($testEntry) {
                $testDirectory = [string]$testEntry[1]
            }
        }
        if (-not $testDirectory) {
            $testDirectory = Join-Path $projectDirectory 'test'
        }

        $hasTestSources = Test-Path -LiteralPath $testDirectory -PathType Container
        if ($hasTestSources) {
            $hasTestSources = @(Get-ChildItem -LiteralPath $testDirectory -File -Recurse -Include '*.c', '*.cc', '*.cpp', '*.cxx').Count -gt 0
        }
        if (-not $hasTestSources) {
            continue
        }

        $defaultEnvironments = @()
        $iniText = Get-Content -Raw -LiteralPath $iniFile.FullName
        $platformMatch = [regex]::Match($iniText, '(?ms)^\[platformio\](.*?)(?=^\[|\z)')
        if ($platformMatch.Success) {
            $defaultMatch = [regex]::Match($platformMatch.Groups[1].Value, '(?m)^\s*default_envs\s*=\s*(.+)$')
            if ($defaultMatch.Success) {
                $defaultEnvironments = @($defaultMatch.Groups[1].Value -split '[,\s]+' | Where-Object { $_ })
            }
        }
        if ($defaultEnvironments.Count -eq 0) {
            $defaultEnvironments = @($environments[0])
        }
        foreach ($environment in $defaultEnvironments) {
            if ($environments -notcontains $environment) {
                throw "PlatformIO test target $relativePath references unknown environment $environment."
            }
            $testTargets.Add([pscustomobject]@{
                    project = $relativePath
                    directory = $projectDirectory
                    environment = $environment
                })
        }
    }

    $script:PlatformIoMatrix = @($matrix)
    $script:PlatformIoTestTargets = @($testTargets)
    Write-Output "Discovered $($script:PlatformIoMatrix.Count) PlatformIO build environments."
    Write-Output "Discovered $($script:PlatformIoTestTargets.Count) PlatformIO test targets."
    $script:PlatformIoMatrix | ForEach-Object { Write-Output "BUILD $($_.project):$($_.environment)" }
    $script:PlatformIoTestTargets | ForEach-Object { Write-Output "TEST $($_.project):$($_.environment)" }
}

function Test-DocsGate {
    param([string]$GateRoot)

    $documentationRoots = @(
        (Join-Path $GateRoot 'README.md'),
        (Join-Path $GateRoot 'docs'),
        (Join-Path $GateRoot '.github'),
        (Join-Path $GateRoot '.serena\memories')
    )
    $markdownFiles = [System.Collections.Generic.List[IO.FileInfo]]::new()
    foreach ($root in $documentationRoots) {
        if (Test-Path -LiteralPath $root -PathType Leaf) {
            $markdownFiles.Add((Get-Item -LiteralPath $root))
        }
        elseif (Test-Path -LiteralPath $root -PathType Container) {
            Get-ChildItem -LiteralPath $root -Filter '*.md' -File -Recurse | ForEach-Object { $markdownFiles.Add($_) }
        }
    }

    # Restrict the audit to Markdown path contexts. This avoids treating prose
    # such as "test/example" as a repository path while still checking code
    # spans and local Markdown links.
    $legacyPattern = '(?m)(^|[`\[(])(dev-info/|test/|tools/(native|portable-smoke|include-fixtures)/|examples/(client-demo|client-demo-wifi|client-demo-ethernet|server-demo|server-bme280-demo|server-io-example|hil-cov-espClient-to-espServer-acceptance|hil-wago-client-acceptance|common)/)'
    foreach ($markdownFile in $markdownFiles) {
        $relative = [IO.Path]::GetRelativePath($GateRoot, $markdownFile.FullName).Replace('\', '/')
        if ($relative -eq 'docs/CHANGELOG.md') {
            continue
        }
        if ((Get-Content -Raw -LiteralPath $markdownFile.FullName) -match $legacyPattern) {
            throw "Current documentation contains a legacy layout reference: $relative"
        }
    }
    Write-Output "Checked $($markdownFiles.Count) documentation and governance Markdown files."
}

function Test-TrackedLayout {
    $trackedPaths = & git -C $repoRoot ls-files
    $script:LastNativeExitCode = $LASTEXITCODE
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to enumerate tracked paths with exit code $LASTEXITCODE."
    }
    $legacyRoots = '^(dev-info|test|tools/native|tools/portable-smoke|tools/include-fixtures|examples/(client-demo|client-demo-wifi|client-demo-ethernet|server-demo|server-bme280-demo|server-io-example|hil-cov-espClient-to-espServer-acceptance|hil-wago-client-acceptance|common))(/|$)'
    $matches = @($trackedPaths | Where-Object { $_ -match $legacyRoots })
    if ($matches.Count -gt 0) {
        throw "Tracked legacy layout roots found: $($matches -join ', ')"
    }
    Write-Output 'No tracked legacy layout roots found.'
}

function Test-TrackedArtifacts {
    $trackedPaths = & git -C $repoRoot ls-files
    $script:LastNativeExitCode = $LASTEXITCODE
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to enumerate tracked paths with exit code $LASTEXITCODE."
    }
    $artifactPattern = '(^|/)(\.pio|\.Temp)(/|$)|^(build|libdeps)/|\.(o|obj|elf|exe|a|lib)$'
    $matches = @($trackedPaths | Where-Object { $_ -match $artifactPattern })
    if ($matches.Count -gt 0) {
        throw "Tracked build artifacts found: $($matches -join ', ')"
    }
    Write-Output 'No tracked build artifacts found.'
}

function New-PackageConsumer {
    param([string]$ConsumerRoot, [string]$PackagePath)

    New-Item -ItemType Directory -Path (Join-Path $ConsumerRoot 'src') -Force | Out-Null
    $consumerIni = @"
[platformio]
description = Fresh package consumer validation
default_envs = usb
build_dir = $($ConsumerRoot.Replace('\', '/'))/build
libdeps_dir = $($ConsumerRoot.Replace('\', '/'))/libdeps

[env:usb]
platform = espressif32
board = nodemcu-32s
framework = arduino
build_unflags =
    -std=gnu++11
build_flags =
    -Wno-deprecated-declarations
    -std=gnu++17
build_src_filter =
    +<consumer_main.cpp>
lib_deps =
    file://$($PackagePath.Replace('\', '/'))
"@
    Set-Content -LiteralPath (Join-Path $ConsumerRoot 'platformio.ini') -Value $consumerIni -Encoding utf8
    Set-Content -LiteralPath (Join-Path $ConsumerRoot 'src\consumer_main.cpp') -Value @'
#include <Arduino.h>
#include <EspBacnet.h>

void setup() {}

void loop() {}
'@ -Encoding utf8
}

function Write-Summary {
    $duration = (Get-Date) - $runStarted
    $passCount = @($script:Results | Where-Object { $_.status -eq 'PASS' }).Count
    $failCount = @($script:Results | Where-Object { $_.status -eq 'FAIL' }).Count
    $skipCount = @($script:Results | Where-Object { $_.status -eq 'SKIP' }).Count
    $summary = [ordered]@{
        schema_version = 1
        repository = $repoRoot
        commit = (& git -C $repoRoot rev-parse HEAD).Trim()
        started_utc = $runStarted.ToUniversalTime().ToString('o')
        duration_ms = [Math]::Round($duration.TotalMilliseconds)
        pass = $passCount
        fail = $failCount
        skipped = $skipCount
        logs_directory = $script:LogsDirectory
        gate_copy = $script:GateRoot
        checks = @($script:Results)
    }
    $summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $script:SummaryPath -Encoding utf8

    Write-Host ''
    Write-Host "PASS: $passCount"
    Write-Host "FAIL: $failCount"
    Write-Host "SKIPPED: $skipCount"
    Write-Host "DURATION: $([Math]::Round($duration.TotalSeconds, 1)) s"
    Write-Host "LOGS: $script:LogsDirectory"
    Write-Host "SUMMARY: $script:SummaryPath"
    if ($script:FailedNames.Count -gt 0) {
        Write-Host 'FAILED CHECKS:'
        $script:FailedNames | Sort-Object | ForEach-Object { Write-Host "  $_" }
    }
    return $summary
}

if ($Clean -and (Test-Path -LiteralPath $tempRoot)) {
    $resolvedTempRoot = (Resolve-Path -LiteralPath $tempRoot).Path.TrimEnd('\')
    if ($resolvedTempRoot -ne $tempRoot) {
        throw "Refusing to clean unexpected path: $resolvedTempRoot"
    }
    Remove-Item -LiteralPath $resolvedTempRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

if ($OnlyFailedFrom) {
    $summaryInput = Resolve-Path -LiteralPath $OnlyFailedFrom -ErrorAction Stop
    $previousSummary = Get-Content -Raw -LiteralPath $summaryInput | ConvertFrom-Json
    $script:OnlyFailedNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    @($previousSummary.checks | Where-Object { $_.status -eq 'FAIL' } | ForEach-Object { [string]$_.name }) | ForEach-Object {
        $script:OnlyFailedNames.Add($_) | Out-Null
    }
    if ($script:OnlyFailedNames.Count -eq 0) {
        throw "No failed checks found in $summaryInput."
    }
}

$commit = (& git -C $repoRoot rev-parse --short HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to determine the current Git commit.'
}
$runId = '{0}-{1}' -f (Get-Date -Format 'yyyyMMdd-HHmmss'), $commit
$runRoot = Join-Path $tempRoot $runId
$script:LogsDirectory = Join-Path $runRoot 'logs'
$script:GateRoot = Join-Path $runRoot 'repository'
$script:SummaryPath = Join-Path $runRoot 'summary.json'
$archivePath = Join-Path $runRoot 'repository.tar'
$cmakeBuild = Join-Path $runRoot 'cmake-build'
$packageDirectory = Join-Path $runRoot 'package'
$consumerRoot = Join-Path $runRoot 'package-consumer'
New-Item -ItemType Directory -Path $script:LogsDirectory, $script:GateRoot, $packageDirectory -Force | Out-Null

if (-not $DiscoverOnly) {
    Invoke-GateCheck -Name 'source-clean' -Command 'git status --porcelain --untracked-files=no' -Action {
        $status = & git -C $repoRoot status --porcelain --untracked-files=no
        $script:LastNativeExitCode = $LASTEXITCODE
        if ($LASTEXITCODE -ne 0) {
            throw "git status failed with exit code $LASTEXITCODE."
        }
        if ($status) {
            throw 'Tracked working tree changes are not allowed for a reproducible gate.'
        }
    } -RequiredSetup
}

Invoke-GateCheck -Name 'gate-copy' -Command 'git archive --format=tar HEAD; tar -xf <archive> -C <gate-copy>' -Action {
    Invoke-NativeCommand -File 'git' -Arguments @('-C', $repoRoot, 'archive', '--format=tar', '--output', $archivePath, 'HEAD')
    Invoke-NativeCommand -File 'tar' -Arguments @('-xf', $archivePath, '-C', $script:GateRoot)
} -RequiredSetup

if (-not (Test-Path -LiteralPath $script:GateRoot -PathType Container)) {
    Skip-GateCheck -Name 'platformio-discovery' -Reason 'Gate copy is unavailable.'
}
else {
    Invoke-GateCheck -Name 'platformio-discovery' -Command 'pio project config --json-output -d <each-platformio-project>' -Action {
        Get-PlatformIoMatrix -GateRoot $script:GateRoot
    } -RequiredSetup
}

if ($DiscoverOnly) {
    $summary = Write-Summary
    exit $(if ($summary.fail -gt 0 -or $summary.skipped -gt 0) { 1 } else { 0 })
}

Invoke-GateCheck -Name 'governance-agent-generator' -Command 'pwsh -File tools/governance/combine-agent-md.ps1' -Action {
    Invoke-NativeCommand -File 'pwsh' -Arguments @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $repoRoot 'tools\governance\combine-agent-md.ps1'))
}
Invoke-GateCheck -Name 'serena-memory-generator' -Command 'pwsh -File tools/governance/serena/combine-serena-shared.ps1' -Action {
    Invoke-NativeCommand -File 'pwsh' -Arguments @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $repoRoot 'tools\governance\serena\combine-serena-shared.ps1'))
}
Invoke-GateCheck -Name 'docs-gate' -Command 'versioned Markdown layout-reference audit in pristine gate copy' -Action {
    Test-DocsGate -GateRoot $script:GateRoot
}
Invoke-GateCheck -Name 'git-diff-check' -Command 'git diff --check' -Action {
    Invoke-NativeCommand -File 'git' -Arguments @('-C', $repoRoot, 'diff', '--check')
}
Invoke-GateCheck -Name 'layout-audit' -Command 'git ls-files layout-root audit' -Action {
    Test-TrackedLayout
}
Invoke-GateCheck -Name 'artifact-audit' -Command 'git ls-files artifact audit' -Action {
    Test-TrackedArtifacts
}
Invoke-GateCheck -Name 'static-precommit' -Command 'pwsh -File tools/quality/run-precommit-full.ps1' -Action {
    Invoke-NativeCommand -File 'pwsh' -Arguments @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $repoRoot 'tools\quality\run-precommit-full.ps1'))
}
Invoke-GateCheck -Name 'cmake-configure' -Command 'cmake -S tools/build/cmake -B <temp-cmake-build>' -Action {
    Invoke-NativeCommand -File 'cmake' -Arguments @('-S', (Join-Path $script:GateRoot 'tools\build\cmake'), '-B', $cmakeBuild)
}
Invoke-GateCheck -Name 'cmake-build' -Command 'cmake --build <temp-cmake-build> --config Release --parallel' -Action {
    Invoke-NativeCommand -File 'cmake' -Arguments @('--build', $cmakeBuild, '--config', 'Release', '--parallel')
}
Invoke-GateCheck -Name 'ctest' -Command 'ctest --test-dir <temp-cmake-build> -C Release --output-on-failure' -Action {
    Invoke-NativeCommand -File 'ctest' -Arguments @('--test-dir', $cmakeBuild, '-C', 'Release', '--output-on-failure')
}

# Keep this explicit and reviewable. It is the intentionally invalid root gate
# that verifies Priority Write cannot be enabled while WriteProperty is disabled.
$expectedNegativeCompileEnvironments = @{
    '.' = @('usb-invalid-priority-gate')
}
if ($script:PlatformIoMatrix.Count -eq 0) {
    Skip-GateCheck -Name 'platformio-build-matrix' -Reason 'PlatformIO discovery produced no environments.'
}
else {
    foreach ($entry in $script:PlatformIoMatrix) {
        $name = "pio-build:$($entry.project):$($entry.environment)"
        $isExpectedNegative = $expectedNegativeCompileEnvironments.ContainsKey($entry.project) -and $expectedNegativeCompileEnvironments[$entry.project] -contains $entry.environment
        Invoke-GateCheck -Name $name -Command (ConvertTo-CommandText -File 'pio' -Arguments @('run', '-d', $entry.directory, '-e', $entry.environment)) -ExpectedFailure:$isExpectedNegative -Action {
            Invoke-NativeCommand -File 'pio' -Arguments @('run', '-d', $entry.directory, '-e', $entry.environment) -WorkingDirectory $script:GateRoot
        }
    }
}

if ($script:PlatformIoTestTargets.Count -eq 0) {
    Skip-GateCheck -Name 'platformio-tests' -Reason 'No PlatformIO test sources were discovered.'
}
else {
    foreach ($entry in $script:PlatformIoTestTargets) {
        $name = "pio-test:$($entry.project):$($entry.environment)"
        Invoke-GateCheck -Name $name -Command (ConvertTo-CommandText -File 'pio' -Arguments @('test', '-d', $entry.directory, '-e', $entry.environment, '--without-uploading', '--without-testing')) -Action {
            Invoke-NativeCommand -File 'pio' -Arguments @('test', '-d', $entry.directory, '-e', $entry.environment, '--without-uploading', '--without-testing') -WorkingDirectory $script:GateRoot
        }
    }
}

$libraryJson = Get-Content -Raw -LiteralPath (Join-Path $script:GateRoot 'library.json') | ConvertFrom-Json
$packagePath = Join-Path $packageDirectory ("ESP32BACnetStack-$($libraryJson.version).tar.gz")
Invoke-GateCheck -Name 'package-export' -Command (ConvertTo-CommandText -File 'pio' -Arguments @('pkg', 'pack', '.', '-o', $packagePath)) -Action {
    Invoke-NativeCommand -File 'pio' -Arguments @('pkg', 'pack', '.', '-o', $packagePath) -WorkingDirectory $script:GateRoot
}
Invoke-GateCheck -Name 'package-content' -Command (ConvertTo-CommandText -File 'python' -Arguments @((Join-Path $script:GateRoot 'tools\release\check-package-content.py'), $packagePath, '--version', $libraryJson.version)) -Action {
    Invoke-NativeCommand -File 'python' -Arguments @((Join-Path $script:GateRoot 'tools\release\check-package-content.py'), $packagePath, '--version', $libraryJson.version) -WorkingDirectory $script:GateRoot
}
Invoke-GateCheck -Name 'package-consumer' -Command 'pio pkg install; pio run -e usb in a fresh package-only consumer' -Action {
    New-PackageConsumer -ConsumerRoot $consumerRoot -PackagePath $packagePath
    Invoke-NativeCommand -File 'pio' -Arguments @('pkg', 'install') -WorkingDirectory $consumerRoot
    Invoke-NativeCommand -File 'pio' -Arguments @('run', '-e', 'usb') -WorkingDirectory $consumerRoot
    $manifest = Get-ChildItem -LiteralPath (Join-Path $consumerRoot 'libdeps\usb') -Filter 'library.json' -File -Recurse |
        Where-Object { (Get-Content -Raw -LiteralPath $_.FullName | ConvertFrom-Json).name -eq $libraryJson.name } |
        Select-Object -First 1
    if (-not $manifest) {
        throw 'Package consumer did not install the expected library manifest.'
    }
    $installed = Get-Content -Raw -LiteralPath $manifest.FullName | ConvertFrom-Json
    if ($installed.version -ne $libraryJson.version) {
        throw "Package consumer installed version $($installed.version), expected $($libraryJson.version)."
    }
    $consumerFull = (Resolve-Path -LiteralPath $consumerRoot).Path.TrimEnd('\')
    $manifestFull = $manifest.FullName
    if (-not $manifestFull.StartsWith($consumerFull + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Package consumer resolved outside its own dependency directory: $manifestFull"
    }
    Write-Output "Installed package version: $($installed.version)"
}

$summary = Write-Summary
if (-not $KeepLogs) {
    # Logs and JSON remain available. Only reproducible build workspaces are
    # removed unless a caller explicitly requests diagnostic retention.
    foreach ($temporaryPath in @($script:GateRoot, $cmakeBuild, $packageDirectory, $consumerRoot, $archivePath)) {
        if (-not (Test-Path -LiteralPath $temporaryPath)) {
            continue
        }
        $resolvedTemporaryPath = (Resolve-Path -LiteralPath $temporaryPath).Path.TrimEnd('\')
        $resolvedRunRoot = (Resolve-Path -LiteralPath $runRoot).Path.TrimEnd('\')
        if (-not $resolvedTemporaryPath.StartsWith($resolvedRunRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove a path outside the current gate run: $resolvedTemporaryPath"
        }
        Remove-Item -LiteralPath $resolvedTemporaryPath -Recurse -Force
    }
    Write-Output "Logs retained at $($script:LogsDirectory)"
    Write-Output "Summary retained at $($script:SummaryPath)"
}
exit $(if ($summary.fail -gt 0 -or $summary.skipped -gt 0) { 1 } else { 0 })
