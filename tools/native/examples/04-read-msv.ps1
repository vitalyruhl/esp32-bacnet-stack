[CmdletBinding()]
param([string]$BindAddress, [string]$Target, [string]$DeviceId, [string]$Instance, [string]$TimeoutMs, [string]$ExeDir, [string]$SettingsPath, [switch]$DryRun)
. (Join-Path $PSScriptRoot '_common.ps1')
try {
    $settings = Import-BacnetExampleSettings -SettingsPath $SettingsPath
    $resolved = Resolve-BacnetConnectionSettings -Settings $settings -BindAddress $BindAddress -Target $Target -DeviceId $DeviceId -TimeoutMs $TimeoutMs -ExeDir $ExeDir
    $instance = Assert-BacnetInstance ([string](Resolve-BacnetSetting $Instance $settings 'MsvInstance')) 'Instance'
    $bin = Get-BacnetNativeBinDirectory $resolved.ExeDir; $exe = Get-BacnetNativeExecutable $bin 'bacnet-client.exe'
    Write-BacnetExampleHeader -Title 'BACnet native example' -BindAddress $resolved.BindAddress -Target $resolved.Target -DeviceId $resolved.DeviceId -ObjectType 'multi-state-value' -Instance $instance -TimeoutMs $resolved.TimeoutMs
    $result = Invoke-BacnetNativeCapture $exe @('--bind',$resolved.BindAddress,'--target',$resolved.Target,'--device-id',$resolved.DeviceId,'--timeout-ms',$resolved.TimeoutMs,'read',("MSV{0}.present-value" -f $instance)) -DryRun:$DryRun
    Assert-BacnetExitCode $result.ExitCode 'MSV Present Value read'
    if (-not $DryRun) { Write-Host ("MSV{0} Present Value = {1}" -f $instance, ($result.Output -join "`n")) }
    exit 0
} catch { Write-BacnetExampleError $_; exit 3 }
