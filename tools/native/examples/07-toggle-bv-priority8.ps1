[CmdletBinding()]
param([string]$BindAddress, [string]$Target, [string]$DeviceId, [string]$Instance, [string]$Priority, [string]$TimeoutMs, [string]$ExeDir, [string]$SettingsPath, [switch]$DryRun, [switch]$Execute)
. (Join-Path $PSScriptRoot '_common.ps1')
try {
    $settings = Import-BacnetExampleSettings -SettingsPath $SettingsPath
    $resolved = Resolve-BacnetConnectionSettings -Settings $settings -BindAddress $BindAddress -Target $Target -DeviceId $DeviceId -TimeoutMs $TimeoutMs -ExeDir $ExeDir
    $instance = Assert-BacnetInstance ([string](Resolve-BacnetSetting $Instance $settings 'BvInstance')) 'Instance'
    $priority = Assert-BacnetPositiveUInt ([string](Resolve-BacnetSetting $Priority $settings 'Priority')) 'Priority'
    if ($priority -ne 8) { throw "07-toggle-bv-priority8.ps1 supports only Priority 8; resolved Priority is $priority." }
    $writeEnabled = $Execute -or [bool]$settings['EnableWriteExamples']
    $bin = Get-BacnetNativeBinDirectory $resolved.ExeDir; $exe = Get-BacnetNativeExecutable $bin 'bacnet-client.exe'
    $base = @('--bind',$resolved.BindAddress,'--target',$resolved.Target,'--device-id',$resolved.DeviceId,'--timeout-ms',$resolved.TimeoutMs)
    Write-BacnetExampleHeader -Title 'BACnet native Priority-8 toggle example' -BindAddress $resolved.BindAddress -Target $resolved.Target -DeviceId $resolved.DeviceId -ObjectType 'binary-value' -Instance $instance -TimeoutMs $resolved.TimeoutMs -Priority $priority -WriteEnabled:$writeEnabled
    if ($DryRun) {
        Write-Host ('DRY-RUN: ' + (Format-BacnetCommand $exe ($base + @('read',("BV{0}.present-value" -f $instance)))))
        Write-Host ('DRY-RUN: ' + (Format-BacnetCommand $exe ($base + @('write-bv-priority',("BV{0}" -f $instance),'<inverted-active-or-inactive>','--priority',$priority,'--execute'))))
        Write-Host ('DRY-RUN: ' + (Format-BacnetCommand $exe ($base + @('priority-slot',("BV{0}" -f $instance),'--priority',$priority))))
        exit 0
    }
    if (-not $writeEnabled) {
        Write-Warning "No WriteProperty sent. Use -Execute once or set EnableWriteExamples = `$true in settings.ps1."
        Write-Host ('Planned: ' + (Format-BacnetCommand $exe ($base + @('write-bv-priority',("BV{0}" -f $instance),'<active|inactive>','--priority',$priority,'--execute'))))
        exit 8
    }
    $currentResult = Invoke-BacnetNativeCapture $exe ($base + @('read',("BV{0}.present-value" -f $instance)))
    Assert-BacnetExitCode $currentResult.ExitCode 'BV Present Value read'
    $current = (($currentResult.Output | Select-Object -Last 1).ToString().Trim().ToLowerInvariant())
    if ($current -ne 'active' -and $current -ne 'inactive') { throw "BV$instance Present Value is '$current', not active or inactive; no write was sent." }
    $targetValue = if ($current -eq 'active') { 'inactive' } else { 'active' }
    $writeCode = Invoke-BacnetNative $exe ($base + @('write-bv-priority',("BV{0}" -f $instance),$targetValue,'--priority',$priority,'--execute'))
    Assert-BacnetExitCode $writeCode 'Priority-8 BV write'
    $readBack = Invoke-BacnetNativeCapture $exe ($base + @('read',("BV{0}.present-value" -f $instance)))
    $slot = if ($readBack.ExitCode -eq 0) { Invoke-BacnetNativeCapture $exe ($base + @('priority-slot',("BV{0}" -f $instance),'--priority',$priority)) } else { $null }
    if ($readBack.ExitCode -ne 0 -or $slot.ExitCode -ne 0) {
        $originalFailure = if ($readBack.ExitCode -ne 0) { $readBack.ExitCode } else { $slot.ExitCode }
        Write-Error "Priority write was acknowledged, but readback failed with exit code $originalFailure. Attempting one cleanup relinquish."
        $cleanupCode = Invoke-BacnetNative $exe ($base + @('relinquish-bv-priority',("BV{0}" -f $instance),'--priority',$priority,'--execute'))
        if ($cleanupCode -eq 0) { Write-Warning 'Cleanup relinquish was acknowledged; the original readback failure remains.'; exit $originalFailure }
        Write-Error "Cleanup relinquish failed with exit code $cleanupCode. Priority 8 may remain active."; exit 9
    }
    Write-Host "BV$instance original=$current target=$targetValue readback=$(($readBack.Output | Select-Object -Last 1))"; $slot.Output | ForEach-Object { Write-Host $_ }
    Write-Warning "Priority 8 remains active for BV$instance. Run .\08-relinquish-bv-priority8.ps1 to release it."
    Write-Host ("Next: .\08-relinquish-bv-priority8.ps1 -Instance $instance -Execute")
    exit 0
} catch { Write-BacnetExampleError $_; exit 3 }
