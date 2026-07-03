# Release Backcheck

The release backcheck verifies that the published PlatformIO package can be
installed and built from a fresh consumer project, without using local
repository sources.

The current check uses `examples/hil-wago-client-acceptance` as the source
template because it is the realistic WAGO BACnet/IP client acceptance example.
The script copies that example into `.Temp/release-backcheck/` and rewrites the
temporary `platformio.ini` to depend on the PlatformIO Registry package:

```ini
lib_deps =
  vitaly.ruhl/ESP32 BACnet Stack@0.18.0
```

The owner identifier is `vitaly.ruhl` in the PlatformIO Registry. That differs
from the GitHub account spelling and is intentionally used for release
verification.

Run the check from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tools/release/backcheck-global-example.ps1
```

The check is successful only when all of these steps pass:

- the temporary consumer project is regenerated under `.Temp/release-backcheck/`
- local-only dependencies such as `symlink://../..`, `lib_extra_dirs`, and
  relative repository source paths are absent
- `pio pkg install` installs `ESP32 BACnet Stack` from the PlatformIO Registry
  at version `0.18.0`
- the installed package resolves inside the temporary consumer project's
  PlatformIO dependency directory, not to repository source files
- `pio run` completes successfully in the temporary consumer project

Do not put real Wi-Fi credentials or WAGO target credentials into the backcheck
project. The script excludes any local `secrets.h` from the template copy and
generates a placeholder `src/secret/secrets.h` from `secrets.example.h` only for
compile-time compatibility.
