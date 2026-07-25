#!/usr/bin/env python3
"""Validate the PlatformIO package archive produced for a release."""

import argparse
import json
import posixpath
import sys
import tarfile


REQUIRED_FILES = {
    "LICENSE",
    "README.md",
    "library.json",
    "docs/CHANGELOG.md",
    "docs/server/README.md",
    "src/BacnetClient.h",
    "src/BacnetServer.h",
    "src/core/server/BacnetServer.cpp",
    "examples/esp32/server/bacnet-server/platformio.ini",
    "examples/esp32/server/bme280/platformio.ini",
}

REQUIRED_PUBLIC_LIBRARY_FILES = {
    "src/ArduinoBacnetClient.h",
    "src/ArduinoBacnetServer.h",
    "src/ArduinoEspBacnet.h",
    "src/BacnetClient.h",
    "src/BacnetDeviceSession.h",
    "src/BacnetDisplayText.h",
    "src/BacnetFeatureGates.h",
    "src/BacnetFixedTextBuffer.h",
    "src/BacnetLogger.h",
    "src/BacnetRemoteObject.h",
    "src/BacnetServer.h",
    "src/EspBacnet.h",
    "src/core/client/BacnetClient.cpp",
    "src/core/client/BacnetClient.h",
    "src/core/client/BacnetDeviceSession.cpp",
    "src/core/client/BacnetDeviceSession.h",
    "src/core/client/BacnetRemoteObject.cpp",
    "src/core/client/BacnetRemoteObject.h",
    "src/core/objects/BacnetAnalogValueLimits.h",
    "src/core/objects/BacnetCommandPriority.h",
    "src/core/objects/BacnetLinearScale.h",
    "src/core/protocol/BacnetProtocol.cpp",
    "src/core/protocol/BacnetProtocol.h",
    "src/core/protocol/BacnetTypes.h",
    "src/core/server/BacnetServer.cpp",
    "src/core/server/BacnetServer.h",
    "src/core/transport/BacnetRuntime.h",
    "src/platform/esp32-arduino/client/ArduinoBacnetClient.cpp",
    "src/platform/esp32-arduino/client/ArduinoBacnetClient.h",
    "src/platform/esp32-arduino/server/ArduinoBacnetServer.h",
    "src/platform/windows/WindowsBacnetDatagramTransport.cpp",
    "src/platform/windows/WindowsBacnetDatagramTransport.h",
    "src/platform/windows/WindowsConsoleLogSink.cpp",
    "src/platform/windows/WindowsConsoleLogSink.h",
    "src/platform/windows/WindowsMonotonicClock.cpp",
    "src/platform/windows/WindowsMonotonicClock.h",
    "src/platform/windows/WindowsSocketRuntime.cpp",
    "src/platform/windows/WindowsSocketRuntime.h",
    "src/portable/BacnetDisplayText.h",
    "src/portable/BacnetAnalogValueLimits.h",
    "src/portable/BacnetCommandPriority.h",
    "src/portable/BacnetLinearScale.h",
    "src/portable/BacnetProtocol.h",
    "src/portable/BacnetRuntime.h",
    "src/portable/BacnetTypes.h",
    "src/support/BacnetDisplayText.h",
    "src/support/BacnetFeatureGates.h",
    "src/support/BacnetFixedTextBuffer.h",
    "src/support/BacnetLogger.cpp",
    "src/support/BacnetLogger.h",
}


def forbidden_reason(path: str) -> str | None:
    parts = path.split("/")
    filename = parts[-1]
    if path.startswith("tests/"):
        return "test consumer"
    if any(
        part in {".git", ".pio", ".pioenvs", ".piolibdeps", ".Temp", ".vscode", ".idea"}
        for part in parts
    ):
        return "local Git, build, temporary, or IDE content"
    if filename in {"secrets.h", "wifiSecret.h", "settings.local.ps1"}:
        return "local secret"
    if filename.endswith(".code-workspace"):
        return "IDE-local file"
    if path.startswith("build/") or (
        "/build/" in path and not path.startswith("tools/build/")
    ):
        return "build output"
    if path.endswith((".o", ".obj", ".elf", ".bin", ".exe", ".a", ".lib")):
        return "compiled artifact"
    return ""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive")
    parser.add_argument("--version", required=True)
    args = parser.parse_args()

    with tarfile.open(args.archive, "r:gz") as package:
        files = {
            posixpath.normpath(entry.name).lstrip("./")
            for entry in package.getmembers()
            if entry.isfile()
        }
        try:
            manifest = json.load(package.extractfile("library.json"))
        except (KeyError, TypeError, json.JSONDecodeError) as error:
            print(f"[E] Unable to read package library.json: {error}")
            return 1

    missing = sorted((REQUIRED_FILES | REQUIRED_PUBLIC_LIBRARY_FILES) - files)
    forbidden = [
        f"{path}: {reason}"
        for path in sorted(files)
        if (reason := forbidden_reason(path))
    ]
    if manifest.get("version") != args.version:
        print(
            f"[E] Package version mismatch: expected {args.version}, "
            f"got {manifest.get('version')}"
        )
        return 1
    if missing:
        print("[E] Missing required package files:")
        print("\n".join(missing))
        return 1
    if forbidden:
        print("[E] Forbidden package files:")
        print("\n".join(forbidden))
        return 1

    print(f"[I] Package content check passed: {len(files)} files, version {args.version}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
