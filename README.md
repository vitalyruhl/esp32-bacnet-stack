# ESP32 BACnet Stack

ESP32 BACnet Stack is an early-stage Arduino/PlatformIO library for BACnet/IP
client and server experiments on ESP32 boards.

The project is published as open-source work in progress. APIs and protocol
coverage are still evolving.

## Current Status

BACnet/IP client APIs are already usable for common read-oriented use cases,
including common process object present-value reads, cached-property access,
read-only process-object helpers, and Analog Value metadata reads, while
advanced discovery, write flows, and server MVP remain future work.

## Implementation Matrix

| Area | Capability | Status | Notes |
| --- | --- | --- | --- |
| Client discovery | Who-Is / I-Am discovery | ✅ Implemented | Core discovery flow available through `BacnetClient`. |
| Client discovery | Known-device session with `BacnetDeviceSession` | ✅ Implemented | Session keeps target identity, drives device-scoped calls, and can be created from a known endpoint or discovered `I-Am` metadata. |
| ReadProperty / values | Generic ReadProperty model | ✅ Implemented | Object + property + optional array index request model is available. |
| ReadProperty / values | Reading known device/object properties | ✅ Implemented | Works for selected known properties and known object IDs. |
| ReadProperty / values | Client-side property cache access | ✅ Implemented | Cached value/status/last-update access is available through `BacnetDeviceSession`, `BacnetProperty`, and `BacnetRemoteObject` helpers. |
| ReadProperty / values | Reading selected `present-value` from known AI/AO/AV, BI/BO/BV, and MI/MO/MV objects | 🟢 Use-case ready | Suitable for practical value monitoring on known process objects. |
| ReadProperty / values | Read-only process-object convenience helpers for known AI/AO/AV, BI/BO/BV, and MI/MO/MV objects | 🟢 Use-case ready | `BacnetProcessObject` exposes typed convenience access to `present-value`, status snapshots, and cached helper reads for common process objects. |
| ReadProperty / values | Read-only Analog Value metadata helpers | ✅ Implemented | Engineering units, min/max present-value, resolution, and COV increment reads are available for practical Analog Value monitoring flows. |
| ReadProperty / values | Reading object health/status from `status-flags`, `event-state`, `reliability`, and `out-of-service` | 🟢 Use-case ready | `BacnetDeviceSession::readObjectStatus()` reads each property safely and preserves per-property status. |
| ReadProperty / values | Derived Normal/Warning/Error/OutOfService/Unknown state | 🟡 Partial | Conservative derivation is implemented; warning/error hardware cases still need targeted HIL validation. |
| ReadProperty / values | Displaying/forwarding selected BACnet values by fallback polling | 🟢 Use-case ready | Property subscription abstraction with fallback polling is available. |
| ReadProperty / values | Typed value decode coverage | 🟡 Partial | Common value paths are supported; full coverage is still expanding. |
| Object discovery | Device `object-list` scan | ✅ Implemented | Scans entries from the remote Device object's `object-list`. |
| Object discovery | Blocking `scanObjectList()` | ✅ Implemented | Source-compatible convenience path remains available. |
| Object discovery | Non-blocking object-list scan job | ✅ Implemented | Loop-driven scan job is available for responsive applications. |
| Object discovery | Optional `object-name`, `description`, `present-value` reads during object-list scan | ✅ Implemented | Optional reads are supported during object-list scan flow. |
| Object discovery | BACnet `property-list` discovery / safe read-all | ✅ Implemented | Caller-buffered APIs discover advertised properties where available and safely attempt each property with per-property status results. |
| Subscriptions | Property subscription abstraction with fallback polling | 🟢 Use-case ready | Practical for cyclic update use cases without SubscribeCOV. |
| Subscriptions | Real SubscribeCOV | ⏳ Planned | Not implemented yet. |
| Writes | WriteProperty | 🚫 Not implemented | No write API shipped in current client runtime. |
| Writes | PresentValue priority write helpers | ⏳ Planned | Future client capability, not currently implemented. |
| Writes | Hardware writes | 🚫 Not implemented | Disabled by default; future explicit opt-in only. |
| Examples / validation | `examples/client-object-list-scan-basic` | ✅ Implemented | Canonical serial-only basic client example for known-target property read, object-list scan, and fallback polling. |
| Examples / validation | `examples/client-demo-wifi` | ✅ Implemented | Preserved WiFi client demo with discovery, scan, process-value updates, a read-only value/status browser view, and a watched Analog Value card. |
| Examples / validation | `examples/client-demo-ETH` | ✅ Implemented | WT32-ETH01 V1.4 Ethernet variant of the full client demo with ConfigManager V4.4.0. |
| Examples / validation | `examples/hil-wago-client-acceptance` | 🧪 Local HIL validated | Local hardware acceptance runner for client scenarios. |
| Examples / validation | HIL scenario S01 non-blocking object-list scan | 🧪 Local HIL validated | Validated on local ESP32/BACnet-IP target setup. |
| Examples / validation | HIL scenario S02 common process present-value reads | 🧪 Local HIL validated | Validated on local ESP32/WAGO BACnet-IP target setup. |
| Examples / validation | HIL scenario S03 common process object status reads | 🧪 Local HIL validated | Validated on local ESP32/WAGO BACnet-IP target setup. |
| Examples / validation | Future HIL scenarios S04-S09 | ⏳ Planned | Present as scenario blocks and skipped by default unless enabled. |
| Server / transports | BACnet/IP server role | 🧱 Placeholder | Placeholder role exists; not a completed server feature set. |
| Server / transports | Server MVP | ⏳ Planned | Planned after client runtime completion milestones. |
| Server / transports | BACnet MS/TP | ⏳ Planned | Scheduled for later transport work. |
| Server / transports | Imported upstream bacnet-stack files | 🚫 Not implemented | Upstream files are not imported in this repository. |

Terminology:

- object-list scan means scanning entries from the remote Device object's `object-list`.
- optional reads during object-list scan means selected reads such as `object-name`, `description`, and `present-value`.
- property-list discovery/read-all means discovering advertised properties of an object where available and safely reading them with one status per property.
- property cache access means using the latest stored value/status/update timestamps from earlier reads or fallback-polled subscriptions.
- for simple use cases that need a few known variables and `present-value` display/forwarding, the current client API is already usable.

Additional status notes:

- A reusable BACnet logging layer is available with application-owned outputs.
- BACnet/IP is the first target.
- ESP32 Configuration Manager is not a core dependency. V4.4.0 is pinned only
  by the two explicitly optional client-demo projects.

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

## WiFi And Ethernet Examples

The richer client demo is split deliberately:

- `examples/client-demo-wifi` preserves the existing ConfigManager-driven WiFi
  behavior.
- `examples/client-demo-ETH` targets the Wireless-Tag WT32-ETH01 V1.4
  (WT32-S1/LAN8720) and compiles ConfigManager WiFi support out.

The basic client, WAGO HIL runner, and server demo also provide an `eth`
environment alongside their existing `usb` environment. For fixed BACnet/IP
installations, wired Ethernet is generally the more stable and appropriate
transport; WiFi remains useful where cabling is unavailable.

The WT32 examples use a generic `eth` build environment and a local
`eth-com6` convenience environment. COM6 matches the current AZ-Delivery
USB-to-serial test adapter but is not a portable assumption. To flash, use
3.3 V UART levels, connect UART0 TX/RX and common ground, hold GPIO0 low while
resetting to enter the bootloader, and provide a stable supply for the ESP32
and Ethernet PHY. See the Ethernet client demo README for exact commands and
wiring.

## screenshots

![Screenshot V0.24.1](docs/screenshots/bnm-V0.24.1.jpg)

## Repository Layout

| Path | Purpose |
| --- | --- |
| `src/` | Library headers and implementation |
| `examples/client-demo-wifi/` | Optional WiFi GUI client demo with discovery, scan, and fallback-polled value updates |
| `examples/client-demo-ETH/` | Optional WT32-ETH01 V1.4 Ethernet GUI client demo |
| `examples/common/` | Shared example-only Ethernet and client-demo implementation helpers |
| `examples/client-object-list-scan-basic/` | Canonical serial-only basic BACnet/IP client example |
| `examples/hil-wago-client-acceptance/` | Local ESP32/WAGO client acceptance HIL runner |
| `examples/server-demo/` | Minimal BACnet server role demo |
| `test/` | PlatformIO Unity tests |
| `docs/` | Project documentation |

Repository setup notes are tracked in
[docs/repository-settings.md](docs/repository-settings.md).

## Minimal Use

```cpp
#include <ArduinoEspBacnet.h>
#include <WiFiUdp.h>

WiFiUDP udp;
ArduinoUdpDatagramTransport transport(udp);
ArduinoMonotonicClock clock;
BacnetClient client(transport, &clock);
BacnetServer server;

void setup() {
  client.begin();
  client.sendWhoIs(BacnetIpEndpoint(255, 255, 255, 255));
  server.begin(1234);
}

void loop() {
  BacnetIAmDevice device;
  if (client.pollIAm(device)) {
    // Discovery result available in device.endpoint, device.deviceInstance, etc.
  }
}
```

## Documentation

Detailed documentation is split by topic:

- [Client Guide](docs/client/README.md)
- [Important Client Notes](docs/client/important.md)
- [Client API](docs/client/api.md)
- [Client Examples](docs/client/examples.md)
- [Server Guide](docs/server/README.md)
- [Planned Server Work](docs/server/planned.md)
- [Repository Settings](docs/repository-settings.md)
- [License Model](docs/license-model.md)

## Minimal Client Example

The library supports simple known-target reads through `BacnetDeviceSession`:

```cpp
#include <ArduinoBacnetClient.h>
#include <BacnetDeviceSession.h>
#include <WiFiUdp.h>

WiFiUDP udp;
ArduinoUdpDatagramTransport transport(udp);
ArduinoMonotonicClock clock;
BacnetClient client(transport, &clock);
BacnetDeviceSession device =
    BacnetDeviceSession::fromEndpoint(client, 1234,
                                      BacnetIpEndpoint(192, 0, 2, 101));

void setup() {
  client.begin();

  BacnetValue value;
  const auto status = device.readProperty(
      device.deviceObject(), BacnetPropertyId::ObjectName, value);

  if (status == BacnetDeviceSessionReadStatus::Ack) {
    Serial.println(value.displayText());
  }
}

void loop() {}
```

## Public Imports

- Client-only Arduino projects include `BacnetClient.h` and
  `ArduinoBacnetClient.h`; they do not include server declarations.
- Server-only Arduino projects include `ArduinoBacnetServer.h`; the server
  remains a placeholder API and does not provide BACnet server services.
- `ArduinoEspBacnet.h` is the optional combined Arduino import.
- `EspBacnet.h` remains a legacy compatibility umbrella and imports both roles;
  use one of the narrower imports in new projects.

## Build Basics

Root build:

```sh
pio run -e usb
```

Compile tests without upload:

```sh
pio test -e usb --without-uploading --without-testing
```

Portable core compile smoke test:

```sh
cmake -S tools/portable-smoke -B build/portable-smoke
cmake --build build/portable-smoke
build/portable-smoke/portable_smoke
```

The smoke target compiles the portable protocol modules without Arduino or
ESP32 headers.

## Native Windows Foundation

The native Windows build provides Winsock UDP transport, a monotonic clock,
console logging, focused localhost-only tests, and two productive CLI tools:
`bacnet-discover` and `bacnet-client`. `bacnet-discover-smoke` remains an
internal smoke/HIL target and is not an end-user program.

Requirements: CMake 3.16+, MSVC with C++17 support, and the Windows SDK.

```powershell
cmake -S tools/portable-smoke -B build/native-windows
cmake --build build/native-windows --config Debug
ctest --test-dir build/native-windows -C Debug --output-on-failure
.\build\native-windows\native\Debug\bacnet-discover-smoke.exe --help
.\build\native-windows\native\Debug\bacnet-discover-smoke.exe --self-test
```

The executables are in the MSVC multi-config output directory
`build/native-windows/native/Debug/`. `--self-test` initializes and closes the
Winsock runtime and transport only; the transport test uses local UDP loopback
on `127.0.0.1` and sends no broadcasts.

To validate a real BACnet/IP network, supply the local interface and its
broadcast address at runtime; do not store site-specific addresses in the
repository:

`bacnet-discover` can also be run without network arguments. It sequentially
tries suitable active IPv4 interfaces and writes each selected bind/broadcast
pair to stderr; discovered devices remain on stdout. Use `--bind <local-ip>`
to restrict the run to one interface, and optionally provide `--broadcast`.

Build the productive Windows binaries with:

```cmd
tools\compile-windows-binaries.cmd Release
```

```powershell
.\build\native-windows\native\Debug\bacnet-discover.exe `
  --bind <local-ip> --broadcast <broadcast-ip> --timeout-ms 5000
```

`bacnet-discover` accepts an optional `--device-id <id>` filter and prints the
endpoint, best-effort Device metadata, and Object List counts. Unsupported
metadata remains visible as `<status>` and does not make discovery fail.

`bacnet-client` resolves a device by `--device-id` through `--broadcast`, or
uses `--target <ip[:port]>` directly. `--bind` is always explicit; no local
address is hard-coded. Its two subcommands are:

```powershell
.\build\native-windows\native\Debug\bacnet-client.exe `
  --bind <local-ip> --broadcast <broadcast-ip> --device-id <id> `
  list --max 20 AV0

.\build\native-windows\native\Debug\bacnet-client.exe `
  --bind <local-ip> --target <ip[:port]> --device-id <id> `
  read AV200.present-value
```

An Object selector combines a type and minimum instance (`AI0`, `AV200`,
`MSV2000`). `list` reads the Device Object List, filters and sorts existing
objects, then limits output with `--max`; it never probes an unbounded instance
range. Successful values and list rows use stdout; diagnostics and list
summaries use stderr. Property aliases include `objectName`, `presentValue`,
`statusFlags`, and `outOfService`.

Discovery returns `0` after at least one matching I-Am, `1` for runtime or
socket errors, `2` for a clean timeout, and `3` for invalid arguments. `list`
uses the same `0`-`3` convention. `read` additionally returns `4` for Reject,
`5` for Abort, and `6` for decode or unsupported-value errors.

Optional compile-time write feature gates:

## Property Subscriptions

`BacnetSubscribeOptions` remains source-compatible with polling subscriptions.
Set `preferCov` to request real BACnet SubscribeCOV. A successful registration
suppresses fallback polling, renews before its configured lifetime ends, and
routes matching COV notifications through the existing property cache and
callback path. Send failures, BACnet Error, Reject, Abort, and registration
timeouts are logged with the COV tag and switch the subscription to its existing
polling fallback. No hardware COV interoperability claim is made without a
separate real-device validation.

- `ESP_BACNET_ENABLE_WRITE_PROPERTY` (default `0`)
- `ESP_BACNET_ENABLE_PRIORITY_WRITE` (default `0`, requires write-property flag)

Build changed or directly affected examples when needed. See [Client Examples](docs/client/examples.md) for example roles and [docs/repository-settings.md](docs/repository-settings.md) for repository setup notes.

WT32-ETH01 V1.4 client demo build:

```sh
pio run -d examples/client-demo-ETH -e eth
```

## Dependency Maintenance

Dependabot is configured for GitHub Actions. PlatformIO platform and library dependency updates are currently manual.

<!-- markdownlint-disable MD033 -->

<br>
<br>

## Donate

<table align="center" width="100%" border="0" bgcolor:=#3f3f3f>
<tr align="center">
<td align="center">
if you prefer a one-time donation

[![donate-Paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://paypal.me/FamilieRuhl)

</td>

<td align="center">
Become a patron, by simply clicking on this button (**very appreciated!**):

[![Become a patron](https://c5.patreon.com/external/logo/become_a_patron_button.png)](https://www.patreon.com/join/6555448/checkout?ru=undefined)

</td>
</tr>
</table>

<br>
<br>

<!-- markdownlint-enable MD033 -->

## License

This project is licensed under `GPL-2.0-or-later WITH GCC-exception-2.0`.
See [LICENSE](LICENSE), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), and
[docs/license-model.md](docs/license-model.md).

Copyright 2026 Vitaly Ruhl.
