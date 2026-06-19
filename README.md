# ESP32 BACnet Stack

ESP32 BACnet Stack is an early-stage Arduino/PlatformIO library for BACnet/IP
client and server experiments on ESP32 boards.

The project is private while the initial API and protocol integration are being
shaped. It is intended to become public around the first usable release.

## Current Status

- Initial library scaffold only.
- Minimal `BacnetClient` and `BacnetServer` role placeholders are available.
- BACnet/IP is the first target.
- BACnet MS/TP is planned for later work.
- No upstream `bacnet-stack` source files are imported yet.
- ESP32 Configuration Manager is not a core dependency. It may be used later
  only in explicitly optional demo examples.

## Goals

- Provide one public `BacnetClient` role for BACnet/IP client workflows.
- Provide one public `BacnetServer` role for BACnet/IP server workflows.
- Keep the core library usable from Arduino and PlatformIO projects.
- Keep BACnet protocol implementation work compatible with
  `GPL-2.0-or-later WITH GCC-exception-2.0`.

## Requirements

- ESP32 board supported by PlatformIO.
- PlatformIO with the Arduino framework.
- C++17 enabled through `-std=gnu++17`.

## Repository Layout

| Path | Purpose |
| --- | --- |
| `src/` | Library headers and implementation |
| `examples/client-demo/` | Minimal BACnet client role demo |
| `examples/server-demo/` | Minimal BACnet server role demo |
| `test/` | PlatformIO Unity tests |
| `docs/` | Project documentation |

Repository setup notes are tracked in
[docs/repository-settings.md](docs/repository-settings.md).

## Minimal Use

```cpp
#include <EspBacnet.h>

BacnetClient client;
BacnetServer server;

void setup() {
  client.begin();
  server.begin(1234);
}

void loop() {
}
```

## Build

Root build:

```sh
pio run -e usb
```

Tests:

```sh
pio test -e usb --without-uploading --without-testing
```

Examples:

```sh
pio run -d examples/client-demo -e usb
pio run -d examples/server-demo -e usb
```

Upload and serial monitor commands are intentionally not part of the default
validation flow because they interact with local hardware.

## Dependency Maintenance

Dependabot is configured for GitHub Actions. GitHub's official Dependabot
ecosystem list does not include PlatformIO as a package ecosystem, so PlatformIO
platform and library dependency updates are currently manual.

## License

This project is licensed under `GPL-2.0-or-later WITH GCC-exception-2.0`.
See [LICENSE](LICENSE), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), and
[docs/license-model.md](docs/license-model.md).

Copyright 2026 Vitaly Ruhl.
