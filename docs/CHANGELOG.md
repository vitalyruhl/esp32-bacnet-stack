# Changelog

This changelog is a curated overview.

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
