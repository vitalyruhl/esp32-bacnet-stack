[CmdletBinding()]
param([string]$BindAddress, [string]$Target, [string]$DeviceId, [string]$Instance, [string]$Priority, [string]$TimeoutMs, [string]$ExeDir, [string]$SettingsPath, [switch]$DryRun, [switch]$Execute)
. (Join-Path $PSScriptRoot '_common.ps1')
try {
    $settings = Import-BacnetExampleSettings -SettingsPath $SettingsPath
    $resolved = Resolve-BacnetConnectionSettings -Settings $settings -BindAddress $BindAddress -Target $Target -DeviceId $DeviceId -TimeoutMs $TimeoutMs -ExeDir $ExeDir
    $instance = Assert-BacnetInstance ([string](Resolve-BacnetSetting $Instance $settings 'BvInstance')) 'Instance'
    $priority = Assert-BacnetPositiveUInt ([string](Resolve-BacnetSetting $Priority $settings 'Priority')) 'Priority'
    if ($priority -ne 8) { throw "08-relinquish-bv-priority8.ps1 supports only Priority 8; resolved Priority is $priority." }
    $writeEnabled = $Execute -or [bool]$settings['EnableWriteExamples']
    $bin = Get-BacnetNativeBinDirectory $resolved.ExeDir; $exe = Get-BacnetNativeExecutable $bin 'bacnet-client.exe'
    $base = @('--bind',$resolved.BindAddress,'--target',$resolved.Target,'--device-id',$resolved.DeviceId,'--timeout-ms',$resolved.TimeoutMs)
    Write-BacnetExampleHeader -Title 'BACnet native Priority-8 relinquish example' -BindAddress $resolved.BindAddress -Target $resolved.Target -DeviceId $resolved.DeviceId -ObjectType 'binary-value' -Instance $instance -TimeoutMs $resolved.TimeoutMs -Priority $priority -WriteEnabled:$writeEnabled
    if ($DryRun) {
        Write-Host ('DRY-RUN: ' + (Format-BacnetCommand $exe ($base + @('relinquish-bv-priority',("BV{0}" -f $instance),'--priority',$priority,'--execute'))))
        Write-Host ('DRY-RUN: ' + (Format-BacnetCommand $exe ($base + @('priority-slot',("BV{0}" -f $instance),'--priority',$priority))))
        Write-Host ('DRY-RUN: ' + (Format-BacnetCommand $exe ($base + @('read',("BV{0}.present-value" -f $instance)))))
        exit 0
    }
    if (-not $writeEnabled) {
        Write-Warning "No WriteProperty sent. Use -Execute once or set EnableWriteExamples = `$true in settings.ps1."
        Write-Host ('Planned: ' + (Format-BacnetCommand $exe ($base + @('relinquish-bv-priority',("BV{0}" -f $instance),'--priority',$priority,'--execute'))))
        exit 8
    }
    $exitCode = Invoke-BacnetNative $exe ($base + @('relinquish-bv-priority',("BV{0}" -f $instance),'--priority',$priority,'--execute'))
    Assert-BacnetExitCode $exitCode 'Priority-8 relinquish'
    $slot = Invoke-BacnetNativeCapture $exe ($base + @('priority-slot',("BV{0}" -f $instance),'--priority',$priority)); Assert-BacnetExitCode $slot.ExitCode 'Priority Array slot 8 read'
    $present = Invoke-BacnetNativeCapture $exe ($base + @('read',("BV{0}.present-value" -f $instance))); Assert-BacnetExitCode $present.ExitCode 'BV Present Value read'
    Write-Host "BV$instance priority 8 relinquished."; $slot.Output | ForEach-Object { Write-Host $_ }; Write-Host "Present Value now follows the next active priority or Relinquish Default: $(($present.Output | Select-Object -Last 1))"
    exit 0
} catch { Write-Error 'Relinquish failed; priority 8 may still be active.'; Write-BacnetExampleError $_; exit 3 }
