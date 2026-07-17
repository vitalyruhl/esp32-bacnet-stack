Set-StrictMode -Version Latest

function Import-BacnetExampleSettings {
    param([string]$SettingsPath)

    $basePath = Join-Path $PSScriptRoot 'settings.ps1'
    if (-not (Test-Path -LiteralPath $basePath -PathType Leaf)) {
        throw "Settings file was not found: $basePath"
    }

    $BacnetSettings = $null
    . $basePath
    if ($null -eq $BacnetSettings -or -not ($BacnetSettings -is [System.Collections.IDictionary])) {
        throw "Settings file '$basePath' must define `$BacnetSettings as a dictionary."
    }

    $localPath = Join-Path $PSScriptRoot 'settings.local.ps1'
    if (Test-Path -LiteralPath $localPath -PathType Leaf) {
        . $localPath
    }
    if (-not [string]::IsNullOrWhiteSpace($SettingsPath)) {
        $explicitPath = [System.IO.Path]::GetFullPath($SettingsPath)
        if (-not (Test-Path -LiteralPath $explicitPath -PathType Leaf)) {
            throw "Settings file was not found: $explicitPath"
        }
        . $explicitPath
    }
    if ($null -eq $BacnetSettings -or -not ($BacnetSettings -is [System.Collections.IDictionary])) {
        throw 'Loaded settings must define $BacnetSettings as a dictionary.'
    }

    Assert-BacnetSettings $BacnetSettings
    return $BacnetSettings
}

function Resolve-BacnetSetting {
    param(
        [AllowEmptyString()][string]$ParameterValue,
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$Settings,
        [Parameter(Mandatory = $true)][string]$Name,
        [string]$EnvironmentName
    )

    if (-not [string]::IsNullOrWhiteSpace($ParameterValue)) { return $ParameterValue }
    if ($Settings.Contains($Name) -and $null -ne $Settings[$Name] -and -not [string]::IsNullOrWhiteSpace([string]$Settings[$Name])) {
        return $Settings[$Name]
    }
    if (-not [string]::IsNullOrWhiteSpace($EnvironmentName)) {
        $fallback = [Environment]::GetEnvironmentVariable($EnvironmentName)
        if (-not [string]::IsNullOrWhiteSpace($fallback)) { return $fallback }
    }
    throw "Missing $Name. Supply the parameter, set it in settings.ps1, or set $EnvironmentName."
}

function Resolve-BacnetOptionalSetting {
    param(
        [AllowEmptyString()][string]$ParameterValue,
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$Settings,
        [Parameter(Mandatory = $true)][string]$Name,
        [string]$EnvironmentName
    )

    if (-not [string]::IsNullOrWhiteSpace($ParameterValue)) { return $ParameterValue }
    if ($Settings.Contains($Name) -and -not [string]::IsNullOrWhiteSpace([string]$Settings[$Name])) { return $Settings[$Name] }
    if (-not [string]::IsNullOrWhiteSpace($EnvironmentName)) {
        $fallback = [Environment]::GetEnvironmentVariable($EnvironmentName)
        if (-not [string]::IsNullOrWhiteSpace($fallback)) { return $fallback }
    }
    return ''
}

function Resolve-BacnetConnectionSettings {
    param(
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$Settings,
        [AllowEmptyString()][string]$BindAddress,
        [AllowEmptyString()][string]$Target,
        [AllowEmptyString()][string]$DeviceId,
        [AllowEmptyString()][string]$TimeoutMs,
        [AllowEmptyString()][string]$ExeDir
    )

    $resolvedBind = Assert-BacnetIpv4 ([string](Resolve-BacnetSetting $BindAddress $Settings 'BindAddress' 'BACNET_BIND')) 'BindAddress'
    $resolvedTarget = Assert-BacnetTarget ([string](Resolve-BacnetSetting $Target $Settings 'Target' 'BACNET_TARGET')) 'Target'
    $resolvedDevice = Assert-BacnetInstance ([string](Resolve-BacnetSetting $DeviceId $Settings 'DeviceId' 'BACNET_DEVICE_ID')) 'DeviceId'
    $resolvedTimeout = Assert-BacnetPositiveUInt ([string](Resolve-BacnetSetting $TimeoutMs $Settings 'TimeoutMs')) 'TimeoutMs'
    $resolvedExeDir = [string](Resolve-BacnetOptionalSetting $ExeDir $Settings 'ExeDir' 'BACNET_NATIVE_BIN')
    return [pscustomobject]@{ BindAddress = $resolvedBind; Target = $resolvedTarget; DeviceId = $resolvedDevice; TimeoutMs = $resolvedTimeout; ExeDir = $resolvedExeDir }
}

function Resolve-BacnetDiscoverySettings {
    param(
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$Settings,
        [AllowEmptyString()][string]$BindAddress,
        [AllowEmptyString()][string]$BroadcastAddress,
        [AllowEmptyString()][string]$TimeoutMs,
        [AllowEmptyString()][string]$ExeDir
    )

    $resolvedBind = Assert-BacnetIpv4 ([string](Resolve-BacnetSetting $BindAddress $Settings 'BindAddress' 'BACNET_BIND')) 'BindAddress'
    $resolvedBroadcast = Assert-BacnetIpv4 ([string](Resolve-BacnetSetting $BroadcastAddress $Settings 'BroadcastAddress' 'BACNET_BROADCAST')) 'BroadcastAddress'
    $resolvedTimeout = Assert-BacnetPositiveUInt ([string](Resolve-BacnetSetting $TimeoutMs $Settings 'TimeoutMs')) 'TimeoutMs'
    $resolvedExeDir = [string](Resolve-BacnetOptionalSetting $ExeDir $Settings 'ExeDir' 'BACNET_NATIVE_BIN')
    return [pscustomobject]@{ BindAddress = $resolvedBind; BroadcastAddress = $resolvedBroadcast; TimeoutMs = $resolvedTimeout; ExeDir = $resolvedExeDir }
}

function Assert-BacnetSettings {
    param([Parameter(Mandatory = $true)][System.Collections.IDictionary]$Settings)

    $required = @('ExeDir','BindAddress','BroadcastAddress','Target','DeviceId','AvInstance','BvInstance','MsvInstance','TimeoutMs','CovLifetimeSeconds','Priority','EnableWriteExamples')
    foreach ($name in $required) {
        if (-not $Settings.Contains($name)) { throw "Missing setting '$name'." }
    }
    Assert-BacnetIpv4 ([string]$Settings['BindAddress']) "setting 'BindAddress'" | Out-Null
    Assert-BacnetIpv4 ([string]$Settings['BroadcastAddress']) "setting 'BroadcastAddress'" | Out-Null
    Assert-BacnetTarget ([string]$Settings['Target']) "setting 'Target'" | Out-Null
    Assert-BacnetInstance ([string]$Settings['DeviceId']) "setting 'DeviceId'" | Out-Null
    Assert-BacnetInstance ([string]$Settings['AvInstance']) "setting 'AvInstance'" | Out-Null
    Assert-BacnetInstance ([string]$Settings['BvInstance']) "setting 'BvInstance'" | Out-Null
    Assert-BacnetInstance ([string]$Settings['MsvInstance']) "setting 'MsvInstance'" | Out-Null
    Assert-BacnetPositiveUInt ([string]$Settings['TimeoutMs']) "setting 'TimeoutMs'" | Out-Null
    Assert-BacnetPositiveUInt ([string]$Settings['CovLifetimeSeconds']) "setting 'CovLifetimeSeconds'" | Out-Null
    $priority = Assert-BacnetPositiveUInt ([string]$Settings['Priority']) "setting 'Priority'"
    if ($priority -gt 16) { throw "Invalid setting 'Priority': '$priority' must be from 1 through 16." }
    if (-not ($Settings['EnableWriteExamples'] -is [bool])) { throw "Invalid setting 'EnableWriteExamples': value must be Boolean." }
    if ($null -eq $Settings['ExeDir']) { throw "Invalid setting 'ExeDir': value must be a string." }
}

function Get-BacnetNativeBinDirectory {
    param([string]$ExeDir)

    if (-not [string]::IsNullOrWhiteSpace($ExeDir)) {
        $resolved = [System.IO.Path]::GetFullPath($ExeDir)
        if ((Test-Path -LiteralPath (Join-Path $resolved 'bacnet-discover.exe')) -and (Test-Path -LiteralPath (Join-Path $resolved 'bacnet-client.exe'))) { return $resolved }
        throw "Native BACnet binaries were not found in: $resolved`nBuild them with:`n  cmake -S tools/portable-smoke -B build/native-windows`n  cmake --build build/native-windows --config Debug`nor pass -ExeDir <path>."
    }
    $candidates = @(
        (Join-Path $PSScriptRoot '..\..\..\build\native-windows\native\Debug'),
        (Join-Path $PSScriptRoot '..\..\..\build\native-windows\native\Release')
    )
    foreach ($candidate in $candidates) {
        $resolved = [System.IO.Path]::GetFullPath($candidate)
        if ((Test-Path -LiteralPath (Join-Path $resolved 'bacnet-discover.exe')) -and (Test-Path -LiteralPath (Join-Path $resolved 'bacnet-client.exe'))) { return $resolved }
    }
    throw 'Native BACnet binaries were not found. Build them with: cmake -S tools/portable-smoke -B build/native-windows; cmake --build build/native-windows --config Debug; or pass -ExeDir <path>.'
}

function Get-BacnetNativeExecutable {
    param([Parameter(Mandatory = $true)][string]$BinDirectory, [Parameter(Mandatory = $true)][string]$Name)
    $path = Join-Path $BinDirectory $Name
    if (-not (Test-Path -LiteralPath $path)) { throw "Required native binary is missing: $path" }
    return $path
}

function Assert-BacnetIpv4 {
    param([Parameter(Mandatory = $true)][string]$Address, [Parameter(Mandatory = $true)][string]$DisplayName)
    $parsed = [System.Net.IPAddress]::None
    if (-not [System.Net.IPAddress]::TryParse($Address, [ref]$parsed) -or $parsed.AddressFamily -ne [System.Net.Sockets.AddressFamily]::InterNetwork) {
        throw "Invalid ${DisplayName}: '$Address' is not a valid IPv4 address."
    }
    return $Address
}

function Assert-BacnetTarget {
    param([Parameter(Mandatory = $true)][string]$Target, [Parameter(Mandatory = $true)][string]$DisplayName)
    if ($Target -notmatch '^(?<address>[^:]+):(?<port>\d+)$') { throw "Invalid ${DisplayName}: '$Target' must use IPv4:port syntax." }
    Assert-BacnetIpv4 $Matches.address $DisplayName | Out-Null
    [uint32]$port = 0
    if (-not [uint32]::TryParse($Matches.port, [ref]$port) -or $port -eq 0 -or $port -gt 65535) { throw "Invalid ${DisplayName}: '$Target' has an invalid UDP port." }
    return $Target
}

function Assert-BacnetInstance {
    param([Parameter(Mandatory = $true)][string]$Value, [Parameter(Mandatory = $true)][string]$DisplayName)
    [uint32]$parsed = 0
    if (-not [uint32]::TryParse($Value, [ref]$parsed) -or $parsed -gt 4194303) { throw "Invalid ${DisplayName}: '$Value' must be an unsigned BACnet instance from 0 through 4194303." }
    return $parsed
}

function Assert-BacnetPositiveUInt {
    param([Parameter(Mandatory = $true)][string]$Value, [Parameter(Mandatory = $true)][string]$DisplayName)
    [uint32]$parsed = 0
    if (-not [uint32]::TryParse($Value, [ref]$parsed) -or $parsed -eq 0) { throw "Invalid ${DisplayName}: '$Value' must be greater than zero." }
    return $parsed
}

function Format-BacnetCommand {
    param([Parameter(Mandatory = $true)][string]$Executable, [string[]]$Arguments)
    $quoted = @($Executable) + ($Arguments | ForEach-Object { if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ } })
    return ($quoted -join ' ')
}

function Invoke-BacnetNative {
    param([Parameter(Mandatory = $true)][string]$Executable, [string[]]$Arguments, [switch]$DryRun)
    if ($DryRun) { Write-Host ('DRY-RUN: ' + (Format-BacnetCommand $Executable $Arguments)); return 0 }
    & $Executable @Arguments | Out-Host
    return $LASTEXITCODE
}

function Invoke-BacnetNativeCapture {
    param([Parameter(Mandatory = $true)][string]$Executable, [string[]]$Arguments, [switch]$DryRun)
    if ($DryRun) { Write-Host ('DRY-RUN: ' + (Format-BacnetCommand $Executable $Arguments)); return [pscustomobject]@{ ExitCode = 0; Output = @() } }
    $output = @(& $Executable @Arguments)
    return [pscustomobject]@{ ExitCode = $LASTEXITCODE; Output = $output }
}

function Assert-BacnetExitCode {
    param([int]$ExitCode, [string]$Operation)
    if ($ExitCode -ne 0) { Write-Error "$Operation failed with native exit code $ExitCode."; exit $ExitCode }
}

function Write-BacnetExampleHeader {
    param([string]$Title, [string]$BindAddress, [string]$Target, [string]$DeviceId, [string]$ObjectType, [string]$Instance, [string]$BroadcastAddress, [string]$TimeoutMs, [string]$Priority, [bool]$WriteEnabled)
    Write-Host $Title
    Write-Host "  Bind:       $BindAddress"
    if (-not [string]::IsNullOrWhiteSpace($BroadcastAddress)) { Write-Host "  Broadcast:  $BroadcastAddress" }
    if (-not [string]::IsNullOrWhiteSpace($Target)) { Write-Host "  Target:     $Target" }
    if (-not [string]::IsNullOrWhiteSpace($DeviceId)) { Write-Host "  Device:     $DeviceId" }
    if (-not [string]::IsNullOrWhiteSpace($ObjectType)) { Write-Host "  Object:     $ObjectType,$Instance" }
    if (-not [string]::IsNullOrWhiteSpace($TimeoutMs)) { Write-Host "  Timeout:    $TimeoutMs ms" }
    if (-not [string]::IsNullOrWhiteSpace($Priority)) { Write-Host "  Priority:   $Priority"; Write-Host ("  Write enabled: " + $(if ($WriteEnabled) { 'yes' } else { 'no' })) }
}

function Write-BacnetExampleError {
    param([Parameter(Mandatory = $true)]$ErrorRecord)
    Write-Error $ErrorRecord.Exception.Message
}
