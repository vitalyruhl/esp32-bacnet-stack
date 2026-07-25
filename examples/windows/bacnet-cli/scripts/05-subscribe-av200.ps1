[CmdletBinding()]
param([string]$BindAddress, [string]$Target, [string]$DeviceId, [string]$Instance, [string]$LifetimeSeconds, [string]$DurationSeconds, [string]$TimeoutMs, [string]$ExeDir, [string]$SettingsPath, [switch]$DryRun)
. (Join-Path $PSScriptRoot '_common.ps1')
try {
    $settings = Import-BacnetExampleSettings -SettingsPath $SettingsPath
    $resolved = Resolve-BacnetConnectionSettings -Settings $settings -BindAddress $BindAddress -Target $Target -DeviceId $DeviceId -TimeoutMs $TimeoutMs -ExeDir $ExeDir
    $instance = Assert-BacnetInstance ([string](Resolve-BacnetSetting $Instance $settings 'AvInstance')) 'Instance'
    $lifetime = Assert-BacnetPositiveUInt ([string](Resolve-BacnetSetting $LifetimeSeconds $settings 'CovLifetimeSeconds')) 'LifetimeSeconds'
    $bin = Get-BacnetNativeBinDirectory $resolved.ExeDir; $exe = Get-BacnetNativeExecutable $bin 'bacnet-client.exe'
    Write-BacnetExampleHeader -Title 'BACnet native SubscribeCOV example' -BindAddress $resolved.BindAddress -Target $resolved.Target -DeviceId $resolved.DeviceId -ObjectType 'analog-value' -Instance $instance -TimeoutMs $resolved.TimeoutMs
    Write-Host "  COV lifetime: $lifetime seconds"
    $arguments = @('--bind',$resolved.BindAddress,'--target',$resolved.Target,'--device-id',$resolved.DeviceId,'--timeout-ms',$resolved.TimeoutMs,'subscribe',("AV{0}" -f $instance),'--lifetime-seconds',$lifetime)
    if (-not [string]::IsNullOrWhiteSpace($DurationSeconds)) {
        $duration = Assert-BacnetPositiveUInt $DurationSeconds 'DurationSeconds'
        $arguments += @('--duration-seconds',$duration)
        Write-Host "  Observation: $duration seconds"
    }
    $exitCode = Invoke-BacnetNative $exe $arguments -DryRun:$DryRun
    Assert-BacnetExitCode $exitCode 'SubscribeCOV'
    exit 0
} catch { Write-BacnetExampleError $_; exit 3 }
