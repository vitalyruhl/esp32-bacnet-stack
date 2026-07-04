# Changelog

This changelog is a curated overview.

## Unreleased

- No unreleased changes documented yet.

## 0.24.1

- Reduced `examples/client-demo` JSON/RAM pressure by limiting displayed
  analog, binary, and multi-state object previews to three entries each,
  shrinking the corresponding scan buffer, and compacting the watched Analog
  Value card's default live payload.
- Reduced additional heap churn in `examples/client-demo` by keeping BACnet
  preview data source-backed where practical and filling BACnet runtime JSON
  through grouped providers instead of per-field `String` callbacks.
- Split the watched Analog Value object name and description in the compact
  runtime card and shortened metadata output by avoiding repeated units.
- Removed the inactive BME280 live sensor card from `examples/client-demo`,
  moved its future BACnet-server draft into `examples/server-demo` behind a
  disabled build block, and moved fallback object type defaults out of the main
  client sketch.
- Kept object descriptions visible in compact object rows when they differ from
  the selected display label.
- Kept library and example version references aligned at `0.24.1`.

## 0.24.0

- Added read-only BACnet process-object convenience helpers, Analog Value
  metadata helpers, reusable BACnet display text helpers, and a watched Analog
  Value card in `examples/client-demo`.
- Kept library and example version references aligned at `0.24.0` for the next
  published release.

## 0.19.0 - 0.23.0

- Added `BacnetDeviceSession::fromEndpoint()` and `fromIAm()`, client-side
  property cache storage/access helpers, and
  `readAllAdvertisedProperties()` support for property-list-driven read-all
  flows.
- Established `examples/client-object-list-scan-basic` as the canonical
  serial-only client example and aligned `examples/client-demo` labels,
  diagnostics, read-only browser behavior, and `Scan / Rescan` flow with the
  current client scope.
- Introduced compile-time write feature gates and kept library/example version
  references aligned through the published `0.23.0` release.

## 0.13.0 - 0.18.0

- Added fallback-polled property subscriptions, reusable BACnet display/status
  helpers, non-blocking and blocking object-list scan flows, and common
  process-object present-value/status coverage for AI/AO/AV, BI/BO/BV, and
  MI/MO/MV.
- Added safe property-list discovery/read-all support, the first read-only
  object health view in `examples/client-demo`, and release backcheck tooling
  for the published `0.18.0` PlatformIO package.
- Extended the local WAGO HIL acceptance runner with `S01` object-list scan,
  `S02` common process present-value reads, and `S03` common process status
  reads while keeping write paths disabled by default.

## 0.1.0 - 0.12.0

- Built the initial BACnet/IP client foundation: discovery, generic
  `ReadProperty`, logging, `BacnetDeviceSession`, `BacnetRemoteObject`,
  `BacnetProperty`, blocking Device `object-list` scan support, and the first
  serial-only object-list validation example.
- Established the ESP32/PlatformIO project scaffold with examples, tests,
  documentation, and the early BACnet client demo validation flow.
