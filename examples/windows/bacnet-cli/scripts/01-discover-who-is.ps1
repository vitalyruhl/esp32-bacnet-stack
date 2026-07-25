[CmdletBinding()]
param([string]$BindAddress, [string]$BroadcastAddress, [string]$TimeoutMs, [string]$ExeDir, [string]$SettingsPath, [switch]$DryRun)
. (Join-Path $PSScriptRoot '_common.ps1')
try {
    $settings = Import-BacnetExampleSettings -SettingsPath $SettingsPath
    $resolved = Resolve-BacnetDiscoverySettings -Settings $settings -BindAddress $BindAddress -BroadcastAddress $BroadcastAddress -TimeoutMs $TimeoutMs -ExeDir $ExeDir
    $bin = Get-BacnetNativeBinDirectory $resolved.ExeDir
    $exe = Get-BacnetNativeExecutable $bin 'bacnet-discover.exe'
    Write-BacnetExampleHeader -Title 'BACnet native discovery' -BindAddress $resolved.BindAddress -BroadcastAddress $resolved.BroadcastAddress -TimeoutMs $resolved.TimeoutMs
    $exitCode = Invoke-BacnetNative $exe @('--bind',$resolved.BindAddress,'--broadcast',$resolved.BroadcastAddress,'--timeout-ms',$resolved.TimeoutMs) -DryRun:$DryRun
    Assert-BacnetExitCode $exitCode 'Who-Is discovery'
    exit 0
} catch { Write-BacnetExampleError $_; exit 3 }
