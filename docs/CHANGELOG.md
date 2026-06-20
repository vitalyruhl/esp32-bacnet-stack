# Changelog

This changelog is a curated overview.

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
  with device instance `9001`.

## 0.1.0

- Initial private scaffold for the ESP32 BACnet/IP stack library.
- Added minimal `BacnetClient` and `BacnetServer` role placeholders.
- Added basic PlatformIO build, examples, tests, CI, and dependency maintenance
  scaffolding.
