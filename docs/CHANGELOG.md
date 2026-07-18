# Changelog

This changelog is a curated overview. The canonical library version is in
`library.json`.

## 0.36.0 - 2026-07-18

### Added

- Added portable, caller-owned read-only Analog Input and Binary Input server
  object profiles with shared ReadProperty, Object_List, Property_List, and
  optional metadata/state-provider support.
- Added `examples/io-example`, a ConfigManager-backed ESP32 WiFi input station
  with LDR, DS18B20, three debounced buttons, and an SSD1306 live display.

### Notes

- This development slice deliberately excludes BACnet outputs, WriteProperty,
  Priority_Array, Relinquish_Default, PWM, output failsafes, alarms, and
  Intrinsic Reporting. Real WiFi/BACnet/OLED HIL is accepted for the read-only
  input profile; future output slices require their own HIL.

## 0.35.0 - 2026-07-18

### Added

- Added the first portable `BacnetServer` runtime foundation with injected
  datagram transport, non-blocking polling, configurable Device identity,
  Who-Is range matching, I-Am responses, and Reject responses for unsupported
  confirmed services.
- Added portable protocol helpers and focused fake-transport coverage for
  Who-Is/I-Am dispatch, source-port preservation, rejected confirmed services,
  malformed datagrams, and server-only native compilation.
- Added the read-only ESP32 WiFi/Ethernet server demo with Device `1682127`,
  stored AV200 sine data, and callback-backed AV201 uptime data.
- Added the ConfigManager-backed, No-OTA BME280 WiFi server demo with
  persisted startup configuration, AV300–AV303 temperature, relative humidity,
  pressure, and Magnus dew-point values, plus a Live View that reads the same
  retained Present_Value data as BACnet.
- Added individually registered caller-owned AV engineering properties
  (Description, Min/Max Present Value, and Resolution) and optional runtime
  Reliability/Status/Limit state without metadata storage in baseline AVs.
- Corrected BME280 pressure publication from hPa to Pascals and added
  non-notifying dew-point limit/hysteresis state exposure.
- Validated Windows BACnet interoperability and Home Assistant discovery with
  real ESP32/BME280 hardware; AV300–AV303 match the Live View.

### Notes

- This release adds no COV, EventNotification/Intrinsic Reporting,
  WriteProperty, priority, or production real-I/O binding to the read-only
  server profile.

## 0.34.0 - 2026-07-17

### Added

- Added a bounded, typed Property Browser to the shared WiFi/Ethernet rich
  client demo. It supports Device, Analog Value, and Multi-State Value objects,
  keeps values typed, and retains a status for every attempted property.
- Added a stepwise, non-blocking browser job with visible load states and at
  most one new BACnet transaction per loop step.
- Added focused Device/Object/Property List support, indexed native CLI array
  selectors, and canonical portable BACnet error names.

### Changed

- Aligned the WiFi `usb` and Ethernet `eth` rich demos on one shared BACnet
  feature set, including identical explicit WriteProperty and priority gates.
- Made the actual I-Am endpoint, including a non-default UDP source port, the
  discovered session endpoint.
- Made remote Object List and Property List reads the primary discovery paths.
  The browser keeps only eight visible rows for RAM and UI bounds.

### Fixed

- Replaced current test/demo references to the obsolete WAGO Device instance
  `9001` with `1682101`; historical release notes remain unchanged.
- Stopped treating `unknown-object` as an unsupported property.
- Removed configured-object substitution from failed Object List scans, so the
  original scan status remains visible.
- Prevented browser loading from blocking the Arduino loop, placing large
  browser results on the loop stack, or allowing background subscriptions to
  overtake a queued browser job indefinitely.

### Validated

- Validated the rich client demos on ESP32 WiFi and WT32-ETH01 Ethernet against
  WAGO Device `1682101`: 72 Object List entries, Device Property List 53,
  AV200 Property List 37, and MSV2020 Property List 30.
- Validated SubscribeCOV and polling, browser responsiveness, and heap/runtime
  stability without a reset or watchdog event; Windows CMake/CTest also passed.

## 0.33.0

- Expanded explicit WriteProperty with BACnet priority `1..16` support, typed
  Priority Array and Relinquish Default reads, and single-priority relinquish.
- Added native Windows PowerShell examples and productive CLI commands for
  discovery, reads, Object List filtering, SubscribeCOV, and explicit
  Priority-8 Binary Value write/relinquish flows.
- Added shared rich-demo monitoring, SubscribeCOV, polling, and manual priority
  controls. Writes remain compile-time and action opt-ins; no path writes a
  Priority Array directly or performs automatic writes.

## 0.25.0 - 0.32.0

- Established the portable BACnet protocol/client core, Arduino adapters, and
  native Windows Winsock transport, CMake targets, and productive CLI tools.
- Added SubscribeCOV registration, notification routing, renewal, and polling
  fallback, plus explicit typed WriteProperty with priority feature gates.
- Split the optional rich client demo into WiFi and WT32-ETH01 Ethernet
  projects while retaining a single shared BACnet/UI application.

## 0.24.1 - 0.24.2

Published checkpoint: `0.24.2`.

- Reduced client-demo JSON, heap, and loop-stack pressure with bounded object
  previews, compact runtime output, and fixed-buffer formatting.
- Stabilized local HIL acceptance flows under larger client runtime chains.

## 0.24.0

- Added read-only BACnet process-object and Analog Value metadata helpers,
  reusable display text, and the first watched Analog Value demo card.

## 0.19.0 - 0.23.0

Published checkpoint: `0.23.0`.

- Added `BacnetDeviceSession` endpoint/I-Am construction, property cache
  access, advertised-property reads, and the canonical basic Object List
  client example.
- Introduced compile-time write feature gates and aligned the early demo scan,
  diagnostics, and read-only browser behavior with the client scope.

## 0.13.0 - 0.18.0

Published checkpoint: `0.18.0`.

- Added polling subscriptions, object-list scans, common process-object value
  and status helpers, and safe Property List discovery/read-all behavior.
- Extended local WAGO HIL coverage for scans, common value reads, and object
  health while retaining write paths disabled by default.

## 0.1.0 - 0.12.0

- Built the initial BACnet/IP client foundation: discovery, generic
  ReadProperty, logging, `BacnetDeviceSession`, `BacnetRemoteObject`,
  `BacnetProperty`, and the first serial Object List example.
- Established the ESP32/PlatformIO project scaffold, tests, documentation, and
  early client-demo validation flow.
