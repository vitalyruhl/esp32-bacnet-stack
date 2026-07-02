# ESP32 BACnet Stack

ESP32 BACnet Stack is an early-stage Arduino/PlatformIO library for BACnet/IP
client and server experiments on ESP32 boards.

The project is published as open-source work in progress. APIs and protocol
coverage are still evolving.

## Current Status

BACnet/IP client APIs are already usable for common read-oriented use cases,
including common process object present-value reads, while advanced discovery,
write flows, and server MVP remain future work.

## Implementation Matrix

| Area | Capability | Status | Notes |
| --- | --- | --- | --- |
| Client discovery | Who-Is / I-Am discovery | ✅ Implemented | Core discovery flow available through `BacnetClient`. |
| Client discovery | Known-device session with `BacnetDeviceSession` | ✅ Implemented | Session keeps target identity and drives device-scoped calls. |
| ReadProperty / values | Generic ReadProperty model | ✅ Implemented | Object + property + optional array index request model is available. |
| ReadProperty / values | Reading known device/object properties | ✅ Implemented | Works for selected known properties and known object IDs. |
| ReadProperty / values | Reading selected `present-value` from known AI/AO/AV, BI/BO/BV, and MI/MO/MV objects | 🟢 Use-case ready | Suitable for practical value monitoring on known process objects. |
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
| Examples / validation | `examples/client-object-list-scan-basic` | ✅ Implemented | Minimal serial-oriented object-list validation example for common process object reads. |
| Examples / validation | `examples/client-demo` | ✅ Implemented | End-to-end client demo with discovery, scan, process-value updates, and a read-only value/status browser view. |
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
- for simple use cases that need a few known variables and `present-value` display/forwarding, the current client API is already usable.

Additional status notes:

- A reusable BACnet logging layer is available with application-owned outputs.
- BACnet/IP is the first target.
- ESP32 Configuration Manager is not a core dependency. It is used only by explicitly optional demo examples.

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

## screenshots

![alt text](docs/screenshots/bnm-V0.8.2.jpg)


## Repository Layout

| Path | Purpose |
| --- | --- |
| `src/` | Library headers and implementation |
| `examples/client-demo/` | Optional GUI client demo with discovery, scan, and fallback-polled value updates |
| `examples/client-object-list-scan-basic/` | Focused object-list scan and value-read example |
| `examples/hil-wago-client-acceptance/` | Local ESP32/WAGO client acceptance HIL runner |
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
  client.sendWhoIs();
  server.begin(1234);
}

void loop() {
  BacnetIAmDevice device;
  if (client.pollIAm(device)) {
    // Discovery result available in device.address, device.deviceInstance, etc.
  }
}
```

## BACnet/IP Client Discovery

The client discovery layer supports:

- builds standard BACnet/IP Who-Is requests
- sends Who-Is on UDP port `47808`
- parses minimal I-Am responses into `BacnetIAmDevice`
- exposes source address, discovered device instance, max APDU, segmentation,
  and vendor ID

The `examples/client-demo` firmware demonstrates discovery on ESP32. It uses
the optional ConfigurationManager-based example setup to connect WiFi, then
sends Who-Is to the local BACnet/IP broadcast address every 30 seconds and logs
received I-Am responses.

Hardware validation for this slice was performed against a WAGO BACnet/IP
server at `<BACNET_DEVICE_IP>:47808`; the ESP32 repeatedly discovered device
instance `<DEVICE_INSTANCE>`.

The exported WAGO BACnet/IP test server configuration and programmed reference
objects are documented in `dev-info/Wago_TestServer/`.

## BACnet/IP Client ReadProperty

The client ReadProperty layer is still intentionally compact, but now uses a
generic property-access model:

- builds minimal confirmed ReadProperty requests
- accepts object type, object instance, property identifier, and optional array
  index through `BacnetPropertyRequest`
- sends ReadProperty to a BACnet/IP device
- parses confirmed ReadProperty ACKs into typed `BacnetValue` results with
  display text conversion for demos and logs
- includes public object, property, request, and value helper types small
  enough to reuse later from server-side work

The initial property targets are device `objectName`, `vendorName`,
`modelName`, and `firmwareRevision`. Hardware validation read those properties
from a WAGO device instance `<DEVICE_INSTANCE>`.

Known remote devices can also be represented with `BacnetDeviceSession`. The
session stores the device instance, address, and BACnet/IP port while
`BacnetClient` remains the transport owner.

For simple reads, `BacnetDeviceSession::readProperty()` provides a blocking
helper around the existing ReadProperty send/poll flow:

```cpp
BacnetDeviceSession device(client, discovered.deviceInstance,
                           discovered.address);

BacnetValue value;
const auto status = device.readProperty(
    device.deviceObject(), BacnetPropertyId::ObjectName, value);

if (status == BacnetDeviceSessionReadStatus::Ack) {
  Serial.println(value.displayText());
}
```

The `examples/client-demo` firmware now keeps BACnet protocol behavior in the
reusable library APIs and limits its own BACnet code to configuration, GUI
binding, display rows, and log adaptation.

`BacnetRemoteObject` and `BacnetProperty` provide lightweight wrappers over
the same session read path:

```cpp
auto mv2000 = device.object(BacnetObjectType::MultiStateValue, 2000);

BacnetValue value;
const auto status = mv2000.readPresentValue(value);

if (status == BacnetDeviceSessionReadStatus::Ack) {
  Serial.println(value.displayText());
}
```

The wrappers do not add scan, cache, queue, or automatic scheduler state.

`BacnetProperty::subscribe()` adds an optional caller-owned property
subscription handle. It uses callback notifications and session-driven fallback
polling. The library does not spawn background tasks; the application drives
polling from `loop()`.

```cpp
auto property = device.object(BacnetObjectType::AnalogValue, 200)
                    .property(BacnetPropertyId::PresentValue);

void onSubscription(const BacnetSubscriptionNotification& n) {
  if (n.status == BacnetDeviceSessionReadStatus::Ack && n.hasValue) {
    Serial.printf("value: %s\n", n.value->displayText());
  }
}

BacnetSubscribeOptions options;
options.fallbackPollMs = 5000;
options.immediateFirstRead = true;

auto sub = property.subscribe(onSubscription, nullptr, options);

void loop() {
  // Run one non-blocking step; call frequently from the main loop.
  device.poll(sub);

  // Optional: trigger an on-demand refresh outside fallback cadence.
  // sub.requestRefresh();
}
```

Subscription notes:

- The caller owns the `BacnetPropertySubscription` handle lifetime.
- The subscription must not outlive the `BacnetDeviceSession` that created it.
- Callbacks run synchronously inside `BacnetDeviceSession::poll()`.
- Notification pointers are valid only during the callback; copy values that
  must be retained.
- One session processes at most one subscription read request in flight.
- `fallbackPollMs = 0` disables periodic fallback polling.
- `requestRefresh()` schedules an immediate one-shot read when idle.
- `stop()` deactivates the subscription and suppresses further polling.

`BacnetDeviceSession::scanObjectList()` keeps the blocking convenience scan over
the remote Device object's `object-list` property. The caller owns the result
buffer and optional filters:

```cpp
const BacnetObjectType valueTypes[] = {
    BacnetObjectType::AnalogValue,
    BacnetObjectType::MultiStateValue,
};

BacnetObjectScanOptions scanOptions;
scanOptions.objectTypes = valueTypes;
scanOptions.objectTypeCount = 2;
scanOptions.maxObjectListEntries = 600;

BacnetScannedObject foundObjects[10];
const auto scan = device.scanObjectList(
    scanOptions, foundObjects, 10);

for (size_t i = 0; i < scan.stored; ++i) {
  auto object = device.object(foundObjects[i].objectId);
  BacnetValue value;
  object.readPresentValue(value);
}
```

The blocking scan is implemented on top of the same small scan job used by the
non-blocking API. Applications that must keep `loop()` responsive can drive the
scan explicitly:

```cpp
static BacnetScannedObject foundObjects[10];
static BacnetObjectListScanJob scanJob;
static const BacnetObjectType valueTypes[] = {
    BacnetObjectType::AnalogValue,
    BacnetObjectType::MultiStateValue,
};

void startScan(BacnetDeviceSession& device) {
  BacnetObjectScanOptions scanOptions;
  bacnetSetObjectTypeFilter(scanOptions, valueTypes);
  scanOptions.maxObjectListEntries = 600;
  scanOptions.readObjectName = true;
  scanOptions.readDescription = true;
  scanOptions.readPresentValue = true;

  device.beginObjectListScan(scanJob, scanOptions, foundObjects, 10);
}

void loopScan(BacnetDeviceSession& device) {
  if (scanJob.isActive()) {
    device.pollObjectListScan(scanJob);
  }

  if (scanJob.isComplete()) {
    const BacnetObjectScanResult& scan = scanJob.summary();
    // Consume foundObjects[0..scan.stored).
  }
}
```

`BacnetObjectListScanJob::progress()` exposes the scan status, phase, current
object-list index, in-flight flag, and summary counters. The scan job stores the
options by value, but pointer fields such as `objectTypes` still reference
caller-owned storage; keep those arrays and the result buffer alive until the
job reaches a terminal state. One session processes one object-list scan job at
a time. Present-value refresh for scanned objects is handled through the
property subscription API with fallback polling.

The `examples/client-object-list-scan-basic` project is a minimal serial-only
hardware validation sketch for known BACnet/IP devices. It uses local
`secret/secrets.h` WiFi/static-IP settings, starts `BacnetClient`, creates a
`BacnetDeviceSession`, scans AV/MV entries with `scanObjectList()`, and prints
compact object id, object-name, description, and present-value output.

The `examples/client-demo` firmware also includes a lightweight BACnet/IP
Discovery card for demo visibility. It selects the configured BACnet/IP device
or the first discovered I-Am device, keeps the BME280 status card unchanged,
and drives a `BacnetObjectListScanJob` from `loop()` for process object
discovery. Up to 10 found Analog, Binary, and Multi-State process objects are
displayed with `description` or `objectName` plus `presentValue` status/value.
The demo also shows one read-only object health/status snapshot selected from
local configuration or the first discovered process object. Present values
refresh through `BacnetProperty::subscribe()` with fallback polling. BACnet scan
and subscription activity is written through the
BACnet logger and forwarded to the ConfigurationManager GUI log by the demo
adapter.

## BACnet Logging

The library exposes a small structured logging layer:

- `BacnetLogLevel`
- `BacnetLogRecord`
- `BacnetLogOutput`
- `BacnetLogger`
- `BacnetScopedLogTag`

Applications attach zero, one, or multiple outputs to
`BacnetClient::logger()`. The core library does not depend on Serial,
ConfigurationManager, MQTT, or file output. Outputs can define their own level,
filter, timestamp mode, and rate limit.

`BacnetLogRecord::message` and `BacnetLogRecord::tag` pointers are valid only
during the `BacnetLogOutput::log()` callback. Buffered outputs must copy those
fields if they keep records for later `tick()` processing.

Logging callsites are controlled by `BACNET_ENABLE_LOGGING`. Debug and trace
style verbose logs are additionally controlled by
`BACNET_ENABLE_VERBOSE_LOGGING`.

## Example Local Configuration

The client demo can read local WiFi and BACnet validation values from an ignored
local secrets file:

```sh
cp examples/client-demo/src/secret/secrets.example.h examples/client-demo/src/secret/secrets.h
```

Edit `examples/client-demo/src/secret/secrets.h` for local WiFi, optional
static IP, optional MAC-priority, and BACnet target values. The `secrets.h` file
is intentionally ignored by Git and must not be committed.

## Build / Validation

Root build:

```sh
pio run -e usb
```

Tests:

```sh
pio test -e usb --without-uploading --without-testing
```

Examples policy:

- Build changed or directly affected examples only.
- For BACnet client runtime/API changes, also build the local acceptance runner.

```sh
pio run -d examples/hil-wago-client-acceptance -e usb
```

Hardware HIL upload/monitor is local-only and should run only with explicit local approval:

```sh
pio run -d examples/hil-wago-client-acceptance -e usb -t upload
pio device monitor -p COM4 -b 115200
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
