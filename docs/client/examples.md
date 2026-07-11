# Client Examples

The repository contains multiple client-facing examples with different purposes.

## `examples/client-object-list-scan-basic`

This is the canonical serial-only basic client example.

It provides both `usb` (WiFi) and `eth` (WT32-ETH01 V1.4) environments and
demonstrates:

- WiFi or Ethernet startup with local secret configuration
- `BacnetClient` startup
- known-target `BacnetDeviceSession` creation
- one Device `object-name` read
- object-list scanning with caller-owned result storage
- one fallback-polled `present-value` subscription
- compact Serial output without ConfigManager or web UI

See [examples/client-object-list-scan-basic/README.md](../../examples/client-object-list-scan-basic/README.md).

## `examples/client-demo-wifi`

This is the preserved WiFi variant of the richer optional client demo with
ConfigManager V4.4.0 integration.

It demonstrates:

- WiFi and UI-driven runtime setup
- Who-Is / I-Am discovery
- session creation for the configured or first discovered BACnet target
- non-blocking object-list scanning
- read-only value and status presentation
- BACnet logger forwarding into the demo GUI log
- runtime rescan action support

The demo remains an example layer. BACnet protocol behavior should stay in the reusable library APIs.

## `examples/client-demo-ETH`

This project builds the same shared BACnet/UI application for the Wireless-Tag
WT32-ETH01 V1.4 and its LAN8720 Ethernet interface. ConfigManager WiFi support
is compiled out; Ethernet address settings are persisted and the WebUI and
BACnet client start after Ethernet has an IP address.

Build with `pio run -d examples/client-demo-ETH -e eth`. The local
`eth-com6` environment adds the current COM6 upload/monitor port without making
that port a generic repository assumption. See the example README for GPIO0,
UART0, power, upload, and monitor instructions.

## `examples/hil-wago-client-acceptance`

This is the local hardware acceptance runner.

It is intended for local ESP32/WAGO validation and may require ignored local secrets and reachable hardware.

Both `usb` and WT32-ETH01 `eth` environments are available. The Ethernet build
uses the same BACnet scenarios and target configuration as the WiFi build.

It should not be treated as the default user-facing example or as a generic validation prerequisite for documentation-only work.
