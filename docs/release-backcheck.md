# Release Backcheck

The release backcheck verifies that the PlatformIO package can be installed and
built from a fresh consumer project, without reusing local repository sources.

By default the check uses `examples/hil-wago-client-acceptance` as the source
template because it is the realistic WAGO BACnet/IP client acceptance example.
The script copies the selected example into `.Temp/release-backcheck/` and
rewrites the temporary `platformio.ini` dependency for the selected environment.
In published mode, the dependency points to the PlatformIO Registry package:

```ini
lib_deps =
  vitaly.ruhl/ESP32 BACnet Stack@<version>
```

The owner identifier is `vitaly.ruhl` in the PlatformIO Registry. That differs
from the GitHub account spelling and is intentionally used for release
verification.

Version behavior:

- without `-Version`, the script reads the default version from `library.json`
- with `-Version "x.y.z"`, the script forces a published registry backcheck for
  that exact version

Run the check from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tools/release/backcheck-global-example.ps1
```

Force a specific published version:

```powershell
powershell -ExecutionPolicy Bypass -File tools/release/backcheck-global-example.ps1 -Version "0.33.0"
```

Select a different example and PlatformIO environment:

```powershell
powershell -ExecutionPolicy Bypass -File tools/release/backcheck-global-example.ps1 -Example "examples/client-object-list-scan-basic" -Environment "usb"
```

Optional local-path mode (not for release verification):

```powershell
powershell -ExecutionPolicy Bypass -File tools/release/backcheck-global-example.ps1 -UseLocalPath
```

The check is successful only when all of these steps pass:

- the temporary consumer project is regenerated under `.Temp/release-backcheck/`
- in published mode, local-only dependencies such as `symlink://../..`,
  `lib_extra_dirs`, and relative repository source paths are absent
- `pio pkg install` resolves the selected dependency and, in published mode,
  installs `ESP32 BACnet Stack` from the PlatformIO Registry at the selected
  version
- in published mode, the installed package resolves inside the temporary
  consumer project's PlatformIO dependency directory, not to repository sources
- `pio run` completes successfully in the temporary consumer project

Do not put real Wi-Fi credentials or WAGO target credentials into the backcheck
project. The script excludes any local `secrets.h` from the template copy and
generates a placeholder `src/secret/secrets.h` from `secrets.example.h` only for
compile-time compatibility.
