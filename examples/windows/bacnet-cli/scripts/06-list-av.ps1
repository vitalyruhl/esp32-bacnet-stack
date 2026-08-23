[CmdletBinding()]
param([string]$BindAddress, [string]$Target, [string]$DeviceId, [string]$TimeoutMs, [string]$ExeDir, [string]$SettingsPath, [switch]$DryRun)
. (Join-Path $PSScriptRoot '_common.ps1')
try {
    $settings = Import-BacnetExampleSettings -SettingsPath $SettingsPath
    $resolved = Resolve-BacnetConnectionSettings -Settings $settings -BindAddress $BindAddress -Target $Target -DeviceId $DeviceId -TimeoutMs $TimeoutMs -ExeDir $ExeDir
    $bin = Get-BacnetNativeBinDirectory $resolved.ExeDir; $exe = Get-BacnetNativeExecutable $bin 'bacnet-client.exe'
    Write-BacnetExampleHeader -Title 'BACnet native Analog Value list' -BindAddress $resolved.BindAddress -Target $resolved.Target -DeviceId $resolved.DeviceId -TimeoutMs $resolved.TimeoutMs
    $exitCode = Invoke-BacnetNative $exe @('--bind',$resolved.BindAddress,'--target',$resolved.Target,'--device-id',$resolved.DeviceId,'--timeout-ms',$resolved.TimeoutMs,'list','--max','4194303','AV0') -DryRun:$DryRun
    Assert-BacnetExitCode $exitCode 'Analog Value list'
    exit 0
} catch { Write-BacnetExampleError $_; exit 3 }
