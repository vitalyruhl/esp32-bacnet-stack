# Project Overview

- Status: verified from the current scaffold.
- esp32-bacnet-stack is a C++17 Arduino/ESP32 BACnet/IP stack library built with
  PlatformIO.
- The current public API direction is one `BacnetClient` role and one
  `BacnetServer` role.
- BACnet/IP is the first implementation target. BACnet MS/TP is planned later.
- ESP32 Configuration Manager is not a core dependency and may only appear later
  in explicitly optional demo examples.
- `library.json` is the canonical package/library metadata and version source.
- Main areas:
  - `src/`: library source plus an export-excluded root build smoke sketch.
  - `examples/`: standalone PlatformIO example applications.
  - `docs/`: user and contributor documentation.
  - `test/`: PlatformIO Unity tests.
  - `tools/`: repository tooling when present.
- `README.md` is the public entry point. `.github/AGENTS.md` is canonical agent
  governance.

Sources: `README.md`, `library.json`, `platformio.ini`, `src/EspBacnet.h`,
`.serena/project.yml`.
