# Changelog

This changelog is a curated overview.

## Unreleased

- Added read-only object status/health snapshots for simple BACnet clients,
  covering `present-value`, `status-flags`, `event-state`, `reliability`, and
  `out-of-service` with conservative Normal/Warning/Error/OutOfService/Unknown
  state derivation.
- Extended `examples/hil-wago-client-acceptance` with `S03` common process
  object status reads, keeping unsupported status properties safe and
  read-only.
- Added a minimal read-only BACnet object health/status view to
  `examples/client-demo`.
- Bumped the library and example version constants to `0.18.0`.

- Added standard BACnet process object identifiers for AI/AO/AV, BI/BO/BV, and
  MI/MO/MV to the generic client-facing object model and text helpers.
- Extended `examples/client-object-list-scan-basic` and
  `examples/client-demo` to scan and read common process object
  `present-value` properties without adding device-specific APIs.
- Extended `examples/hil-wago-client-acceptance` `S02` with optional local
  AI/AO/BI/BO/BV/MI/MO/MV target coverage that skips unconfigured optional
  targets and does not perform BACnet writes.
- Bumped the library and example version constants to `0.17.0`.

- Added caller-buffered BACnet `property-list` discovery and safe read-all APIs
  for known objects with per-property status reporting and preserved typed
  `BacnetValue` results where decoding succeeds.
- Extended `examples/hil-wago-client-acceptance` with optional local `S02`
  property-list discovery and safe read-all validation without BACnet writes.
- Bumped the library and example version constants to `0.16.0`.

- Added `BacnetObjectListScanJob` with `beginObjectListScan()` and
  `pollObjectListScan()` for loop-driven, caller-buffered Device `object-list`
  scans.
- Preserved blocking `scanObjectList()` compatibility by routing it through the
  same scan job state machine.
- Updated `examples/client-demo` to drive object-list discovery from the
  non-blocking scan API.
- Bumped the library and example version constants to `0.15.0`.

- Refactored `examples/client-demo` into a thin ConfigurationManager/UI demo
  over reusable `BacnetDeviceSession`, object-list scan, property, subscription,
  and logging APIs.
- Removed demo-owned BACnet object-list scan state, range probing, active
  ReadProperty invoke tracking, and manual present-value refresh loops from the
  client demo.

- Added `BacnetProperty::subscribe()` with a caller-owned
  `BacnetPropertySubscription` handle for callback-based property updates.
- Added `BacnetDeviceSession::poll()` overloads for non-blocking session-side
  subscription processing with one in-flight read at a time.
- Added fallback polling controls (`fallbackPollMs`, `requestRefresh()`,
  `stop()`, `immediateFirstRead`) and notification reasons for value/status
  transitions.
- Added busy-state protection and move/destructor cleanup for caller-owned
  subscription handles.
- Added focused unit coverage for subscription options, move-only handle
  semantics, refresh behavior, and stop behavior.

- Switched example BACnet target configuration from octet macros to readable
  string-based IP macros with explicit parse-time validation logs.
- Added reusable zero-allocation BACnet text helpers for object types and read
  status in the public library API.
- Added an array-safe helper for object-type scan filters so object-type count
  is inferred from C++ array size instead of manual assignment.
- Aligned example version constants with the canonical library version from
  `library.json`.

## 0.12.0

- Added `examples/client-object-list-scan-basic` as a minimal serial-only
  hardware validation sketch for the `BacnetDeviceSession`,
  `BacnetRemoteObject`, and `scanObjectList()` APIs.

## 0.11.0

- Added blocking `BacnetDeviceSession::scanObjectList()` for caller-buffered
  Device `object-list` scans with optional generic BACnet object-type filters
  and optional object-name, description, and present-value reads.

## 0.10.0

- Added `BacnetRemoteObject` and `BacnetProperty` as lightweight synchronous
  wrappers over `BacnetDeviceSession::readProperty()` for object/property reads.

## 0.9.0

- Added `BacnetDeviceSession` as a lightweight client-owned helper for known
  remote BACnet/IP devices and device-scoped ReadProperty calls.

## 0.8.2

- Improved the client demo BACnet/IP Discovery card so Analog Value and
  Multi-State Value entries render as compact left-aligned vertical lists.
- Added WAGO BACnet/IP test server export data with documented programmed AV,
  BV, and MV reference objects for demo validation.

## 0.8.1

- Captured the UDP source address from I-Am responses so the client demo scans
  the discovered BACnet device instead of relying on a configured device
  instance.
- Added guardrails that warn and skip ReadProperty scans if the selected
  target address is still `0.0.0.0`.
- Kept the configured BACnet target address behind an explicit demo fallback
  switch for parse-only or synthetic discovery records.

## 0.8.0

- Updated the client demo to discover Analog Value and Multi-State Value
  objects from the selected device's Device Object `object-list` property.
- Kept AV200/MV2000 range probing as a disabled debug fallback instead of the
  normal discovery strategy.
- Added WiFi-friendly, demo-local scan timing constants for discovery wait,
  object-list reads, ReadProperty reads, inter-request delay, inspected
  object-list entries, and displayed objects.
- Set the default client-demo object-list inspection cap to 600 entries so
  later WAGO MV objects can be reached while enumeration remains bounded.
- Decoupled the client-demo object-list scan scheduler from periodic Who-Is
  rediscovery so enumeration advances on normal loop ticks.
- Added bounded retries for the client-demo object-list count read so a single
  WiFi timeout does not finish discovery prematurely.
- Documented that object browsing state stays outside the low-level
  `BacnetClient` core.

## 0.7.0

- Added a reusable BACnet logging layer with `BacnetLogLevel`,
  `BacnetLogRecord`, `BacnetLogOutput`, `BacnetLogger`, scoped tags, global and
  per-output levels, per-output filters, timestamp modes, and rate limits.
- Added compile-time logging switches `BACNET_ENABLE_LOGGING` and
  `BACNET_ENABLE_VERBOSE_LOGGING`.
- Wired compact client logs for begin/end, Who-Is, I-Am, ReadProperty send,
  ACK, error, decode rejection, and timeout notifications.
- Added a small client-demo adapter that forwards BACnet library logs to the
  existing ConfigurationManager logging output without adding a ConfigManager
  dependency to the BACnet library.

## 0.6.0

- Added a generic client ReadProperty request model with object identifier,
  property identifier, and optional array-index fields.
- Added typed `BacnetValue` decoding for the supported client ReadProperty
  slice while preserving display text conversion for demos and logs.
- Updated the client demo to scan Analog Value `200..299` and Multi-State Value
  `2000..2099` objects on the first discovered device and display up to 10
  found values for each type.

## 0.5.0

- Added narrow client ReadProperty array-index support for `objectList`.
- Updated the client demo BACnet/IP Discovery card to show the first
  discovered device only, scan Analog Value and Multi-State Value instance
  ranges, and display found `presentValue` values/status without fixed MV
  placeholders.
- Added a client-demo compatibility include for Linux CI builds of the
  ConfigurationManager dependency.

## 0.4.0

- Added minimal client ReadProperty support for multi-state-value
  `presentValue` values.
- Extended the client demo to read WAGO MV `presentValue` values for a small
  temporary hardware-validation object list.

## 0.3.0

- Started minimal BACnet/IP client ReadProperty support with request building
  and narrow ACK parsing for simple character string properties.

## 0.2.0

- Started GitHub Issue #4 with minimal BACnet/IP client discovery support.
- Added BACnet/IP Who-Is request building and sending.
- Added minimal BACnet/IP I-Am response parsing for device discovery.
- Added ESP32 client demo hardware validation against a WAGO BACnet/IP device
  with device instance `<DEVICE_INSTANCE>`.

## 0.1.0

- Initial private scaffold for the ESP32 BACnet/IP stack library.
- Added minimal `BacnetClient` and `BacnetServer` role placeholders.
- Added basic PlatformIO build, examples, tests, CI, and dependency maintenance
  scaffolding.
