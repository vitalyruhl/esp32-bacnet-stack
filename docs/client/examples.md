# Client Examples

The repository contains multiple client-facing examples with different purposes.

## `examples/client-object-list-scan-basic`

This is the canonical serial-only basic client example.

It demonstrates:

- WiFi startup with local secret configuration
- `BacnetClient` startup
- known-target `BacnetDeviceSession` creation
- one Device `object-name` read
- object-list scanning with caller-owned result storage
- one fallback-polled `present-value` subscription
- compact Serial output without ConfigManager or web UI

See [examples/client-object-list-scan-basic/README.md](../../examples/client-object-list-scan-basic/README.md).

## `examples/client-demo`

This is the richer optional client demo with ConfigurationManager integration.

It demonstrates:

- WiFi and UI-driven runtime setup
- Who-Is / I-Am discovery
- session creation for the configured or first discovered BACnet target
- non-blocking object-list scanning
- read-only value and status presentation
- BACnet logger forwarding into the demo GUI log
- runtime rescan action support

The demo remains an example layer. BACnet protocol behavior should stay in the reusable library APIs.

## `examples/hil-wago-client-acceptance`

This is the local hardware acceptance runner.

It is intended for local ESP32/WAGO validation and may require ignored local secrets and reachable hardware.

It should not be treated as the default user-facing example or as a generic validation prerequisite for documentation-only work.
