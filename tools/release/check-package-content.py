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
    "src/BacnetServer.cpp",
    "examples/server-demo/platformio.ini",
    "examples/server-bme280-demo/platformio.ini",
}

REQUIRED_PUBLIC_LIBRARY_FILES = {
    "src/ArduinoBacnetClient.h",
    "src/ArduinoBacnetServer.h",
    "src/ArduinoEspBacnet.h",
    "src/BacnetClient.cpp",
    "src/BacnetClient.h",
    "src/BacnetDeviceSession.cpp",
    "src/BacnetDeviceSession.h",
    "src/BacnetDisplayText.h",
    "src/BacnetFeatureGates.h",
    "src/BacnetFixedTextBuffer.h",
    "src/BacnetLogger.cpp",
    "src/BacnetLogger.h",
    "src/BacnetRemoteObject.cpp",
    "src/BacnetRemoteObject.h",
    "src/BacnetServer.cpp",
    "src/BacnetServer.h",
    "src/EspBacnet.h",
    "src/platform/arduino/ArduinoBacnetClient.cpp",
    "src/platform/arduino/ArduinoBacnetClient.h",
    "src/platform/windows/WindowsBacnetDatagramTransport.cpp",
    "src/platform/windows/WindowsBacnetDatagramTransport.h",
    "src/platform/windows/WindowsConsoleLogSink.cpp",
    "src/platform/windows/WindowsConsoleLogSink.h",
    "src/platform/windows/WindowsMonotonicClock.cpp",
    "src/platform/windows/WindowsMonotonicClock.h",
    "src/platform/windows/WindowsSocketRuntime.cpp",
    "src/platform/windows/WindowsSocketRuntime.h",
    "src/portable/BacnetAnalogValueLimits.h",
    "src/portable/BacnetDisplayText.h",
    "src/portable/BacnetProtocol.cpp",
    "src/portable/BacnetProtocol.h",
    "src/portable/BacnetRuntime.h",
    "src/portable/BacnetTypes.h",
}


def forbidden_reason(path: str) -> str | None:
    parts = path.split("/")
    filename = parts[-1]
    if path.startswith("tools/native/test/"):
        return "native test consumer"
    if any(
        part in {".git", ".pio", ".pioenvs", ".piolibdeps", ".Temp", ".vscode", ".idea"}
        for part in parts
    ):
        return "local Git, build, temporary, or IDE content"
    if filename in {"secrets.h", "wifiSecret.h", "settings.local.ps1"}:
        return "local secret"
    if filename.endswith(".code-workspace"):
        return "IDE-local file"
    if path.startswith("build/") or "/build/" in path:
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
