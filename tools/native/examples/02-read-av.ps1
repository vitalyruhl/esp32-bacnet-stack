[CmdletBinding()]
param([string]$BindAddress, [string]$Target, [string]$DeviceId, [string]$Instance, [string]$TimeoutMs, [string]$ExeDir, [string]$SettingsPath, [switch]$DryRun)
. (Join-Path $PSScriptRoot '_common.ps1')
try {
    $settings = Import-BacnetExampleSettings -SettingsPath $SettingsPath
    $resolved = Resolve-BacnetConnectionSettings -Settings $settings -BindAddress $BindAddress -Target $Target -DeviceId $DeviceId -TimeoutMs $TimeoutMs -ExeDir $ExeDir
    $instance = Assert-BacnetInstance ([string](Resolve-BacnetSetting $Instance $settings 'AvInstance')) 'Instance'
    $bin = Get-BacnetNativeBinDirectory $resolved.ExeDir; $exe = Get-BacnetNativeExecutable $bin 'bacnet-client.exe'
    Write-BacnetExampleHeader -Title 'BACnet native example' -BindAddress $resolved.BindAddress -Target $resolved.Target -DeviceId $resolved.DeviceId -ObjectType 'analog-value' -Instance $instance -TimeoutMs $resolved.TimeoutMs
    $result = Invoke-BacnetNativeCapture $exe @('--bind',$resolved.BindAddress,'--target',$resolved.Target,'--device-id',$resolved.DeviceId,'--timeout-ms',$resolved.TimeoutMs,'read',("AV{0}.present-value" -f $instance)) -DryRun:$DryRun
    Assert-BacnetExitCode $result.ExitCode 'AV Present Value read'
    if (-not $DryRun) { Write-Host ("AV{0} Present Value = {1}" -f $instance, ($result.Output -join "`n")) }
    exit 0
} catch { Write-BacnetExampleError $_; exit 3 }
