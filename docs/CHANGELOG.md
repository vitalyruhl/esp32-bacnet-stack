# Changelog

This changelog is a curated overview.

## 0.28.0

- Added a native Windows Winsock UDP transport, monotonic clock, and console
  log sink for the portable BACnet client foundation.
- Added native CMake targets, localhost-only transport tests, and the
  `bacnet-discover-smoke` Who-Is/I-Am validation program. Property reads,
  object-list scans, and the complete discovery CLI remain planned for #75 and
  #76.

## 0.27.0

- Separated portable client and server imports from Arduino/ESP32 adapters.
- Added Arduino UDP transport, monotonic clock, endpoint conversion, and Serial
  log-output adapters without adding a server runtime.
- Added role-specific Arduino import compile fixtures and retained `EspBacnet.h`
  as the legacy combined Arduino umbrella.

## 0.26.0

- Extracted Arduino-free BACnet protocol types, BVLC/NPDU/APDU codecs, and
  common value/object display helpers into portable modules.
- Added portable endpoint, datagram transport, monotonic-clock, and logging
  boundaries while keeping the existing Arduino `BacnetClient` API as a
  compatibility layer.
- Added a CMake portable compile smoke target that builds the protocol modules
  without Arduino or ESP32 headers. This is compile coverage only; native
  Windows transport and CLI support remain planned work.
- Made the WiFi demo's existing `ESP.h` compatibility wrapper available to
  external PlatformIO dependencies so the pinned ConfigManager build remains
  case-sensitive filesystem compatible.

## 0.25.0

- Split the optional client demo into explicit WiFi and WT32-ETH01 V1.4
  Ethernet projects while keeping the shared BACnet/UI application in one
  example-only source tree.
- Added reusable WT32-ETH01 Ethernet startup support and `eth` build
  environments for the basic client, WAGO HIL runner, and server demo.
- Updated optional demo integration to ConfigManager V4.4.0 and documented the
  local COM6/GPIO0 serial flashing workflow.
- Kept BACnet UDP port behavior, Who-Is/I-Am discovery, and the existing WiFi
  environments unchanged.
- Stabilized the shared client demo's BACnet request ownership, aligned the
  default WAGO Device instance, and retained configured AV/BV/MV reference
  objects when the bounded object-list preview fills before every category.

## 0.24.2

- Reduced stack pressure in BACnet value parsing paths in `BacnetClient` by
  replacing `snprintf`-heavy numeric formatting with lightweight fixed-buffer
  formatting helpers in hot read-property decode paths.
- Increased loop task stack for
  `examples/hil-wago-client-acceptance` (`ARDUINO_LOOP_STACK_SIZE=16384`) to
  keep local HIL acceptance scenarios stable under larger runtime call chains.
- Kept library and example version references aligned at `0.24.2`.

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
- Moved `examples/client-demo` BACnet logging bridge code out of the main
  sketch, added a reusable BACnet log-level prefix helper, added a demo logging
  compile switch, and limited the demo logger output slots to the single output
  it uses.
- Made the watched Analog Value metadata runtime field optional so the default
  client demo payload avoids rebuilding and transferring that detail string on
  each refresh.
- Added small reusable BACnet helpers for fixed-buffer object names, scanned
  labels, numeric value range checks, and object status predicates.
- Documented the planned known-point client ergonomics direction without adding
  a large new client facade or write API.
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
