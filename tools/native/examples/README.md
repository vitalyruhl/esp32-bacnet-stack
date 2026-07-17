# Native Windows BACnet Examples

These PowerShell 5.1+/7+ examples use the repository's native Windows BACnet
programs. They do not create BACnet packets or sockets in PowerShell.

## Quick start

1. Open [settings.ps1](settings.ps1) and adjust the network, device, and object
   values for the intended test environment.
2. Build the native binaries:

   ```powershell
   cmake -S tools/portable-smoke -B build/native-windows
   cmake --build build/native-windows --config Debug
   ```

3. Start an example from this directory:

   ```powershell
   .\01-discover-who-is.ps1
   .\02-read-av.ps1
   .\03-read-bv.ps1
   .\04-read-msv.ps1
   .\05-subscribe-av200.ps1
   .\06-list-av.ps1
   ```

`settings.ps1` contains example values from the repository test environment.
They are not general BACnet defaults. Leave `ExeDir` empty to use the automatic
Debug/Release build-output search.

| Setting | Purpose |
| --- | --- |
| `ExeDir` | Optional directory containing both native `.exe` files. |
| `BindAddress` | Local BACnet/IP IPv4 interface. |
| `BroadcastAddress` | IPv4 broadcast address for Who-Is. |
| `Target` | Default remote device as `IPv4:port`. |
| `DeviceId` | Default BACnet Device instance. |
| `AvInstance` | Default Analog Value instance. |
| `BvInstance` | Default Binary Value instance. |
| `MsvInstance` | Default Multi-State Value instance. |
| `TimeoutMs` | Native request timeout in milliseconds. |
| `CovLifetimeSeconds` | SubscribeCOV lifetime. |
| `Priority` | Priority used by the fixed Priority-8 write examples. Must be `8`. |
| `EnableWriteExamples` | Set to `$true` only to enable the two write examples centrally. |

For local, uncommitted values, create `settings.local.ps1` beside
`settings.ps1`. It is loaded after the base file and is ignored by Git. It can
change individual entries, for example:

```powershell
$BacnetSettings['BindAddress'] = '192.168.10.20'
$BacnetSettings['Target'] = '192.168.10.50:47808'
```

An explicit settings file is loaded last and has the same override behavior:

```powershell
.\02-read-av.ps1 -SettingsPath C:\BACnet\my-settings.ps1
```

For every value, explicit script parameters take precedence over settings;
environment variables (`BACNET_BIND`, `BACNET_BROADCAST`, `BACNET_TARGET`,
`BACNET_DEVICE_ID`, and `BACNET_NATIVE_BIN`) are used only when the respective
setting is empty.

## Examples and overrides

Discovery uses BACnet **Who-Is** and displays received **I-Am** messages:

```powershell
.\01-discover-who-is.ps1
.\01-discover-who-is.ps1 -BroadcastAddress 192.168.10.255
```

AV, BV, and MSV mean Analog Value, Binary Value, and Multi-State Value:

```powershell
.\02-read-av.ps1
.\02-read-av.ps1 -Instance 250
.\03-read-bv.ps1
.\03-read-bv.ps1 -Instance 350
.\04-read-msv.ps1
.\04-read-msv.ps1 -Instance 2021
.\05-subscribe-av200.ps1
.\05-subscribe-av200.ps1 -Instance 250
.\06-list-av.ps1
.\06-list-av.ps1 -Target 192.168.10.20:47808 -DeviceId 5001
```

The SubscribeCOV example displays only received COV values and does not fall
back to polling. Stop it with Ctrl+C. Ctrl+C ends the local subscription runner;
the available native API does not provide remote subscription cancellation.

Every script prints its resolved values before the native command. `-DryRun`
loads and validates settings, prints those values and the native command, but
does not start an `.exe`, open a socket, or send BACnet traffic:

```powershell
.\01-discover-who-is.ps1 -DryRun
.\02-read-av.ps1 -DryRun
.\03-read-bv.ps1 -DryRun
.\04-read-msv.ps1 -DryRun
.\05-subscribe-av200.ps1 -DryRun
.\06-list-av.ps1 -DryRun
.\07-toggle-bv-priority8.ps1 -DryRun
.\08-relinquish-bv-priority8.ps1 -DryRun
```

## Priority-8 Binary Value examples

The write examples use `BvInstance` and `Priority` from settings. `Priority`
must remain `8` because these are fixed Priority-8 examples. Writes are disabled
by default. Enable one write once with `-Execute`, or consciously set
`EnableWriteExamples = $true` in `settings.ps1` (or `settings.local.ps1`):

```powershell
.\07-toggle-bv-priority8.ps1 -Execute
.\08-relinquish-bv-priority8.ps1 -Execute
```

The toggle sends exactly one Priority-8 override after reading and inverting an
`active` or `inactive` BV. The override remains active after success. Always
run the separate relinquish example afterward:

```powershell
.\07-toggle-bv-priority8.ps1
.\08-relinquish-bv-priority8.ps1
```

Relinquish sends BACnet Null for `Present_Value` at Priority 8. Neither example
writes `priority-array`, resets another slot, or retries writes. Both native
commands still require their internal `--execute` gate, and compile-time write
and priority gates must be enabled.

Without `-Execute` or `EnableWriteExamples = $true`, a write example prints its
planned command, sends no write, and returns exit code `8`.

## Exit codes

Native exit codes are: `0` success, `1` runtime/error, `2` timeout, `3` invalid
arguments, `4` Reject, `5` Abort, `6` decode error, `7` feature disabled, and
`8` missing native write opt-in. The PowerShell examples use exit code `3` for
settings or argument errors. The toggle uses `9` when its single cleanup
relinquish also fails and Priority 8 may remain active.

Long parameter lists remain available for special cases; for example:

```powershell
.\02-read-av.ps1 -BindAddress 192.168.2.220 -Target 192.168.2.101:47808 -DeviceId 1682101 -Instance 200
```
