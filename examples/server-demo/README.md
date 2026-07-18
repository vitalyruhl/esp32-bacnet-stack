# BACnet Server Demo

This demo runs the portable read-only `BacnetServer` through the existing
Arduino UDP adapter. The `usb` environment connects an ESP32 through WiFi;
the `eth` environment compiles the same Device and Analog Value profile for a
WT32-ETH01 Ethernet server. Network initialization is separate, while the
BACnet object configuration is shared.

The demo listens on BACnet/IP UDP port `47808` and exposes Device `1682127`:

- AV200 is a stored sine value in degrees Celsius. It uses offset `20`,
  amplitude `10`, period `60 s`, and is updated at most every `250 ms` through
  the normal server object model.
- AV201 is a polling/callback value in seconds. Its provider returns elapsed
  uptime and does not keep a second stored Present Value copy.

Vendor ID `555` and Vendor Name `Unregistered BACnet Test Server` are public
example identity, not secrets. `555` is ASHRAE-reserved for local test/example
use only; it is not a private BACnet number range. Product providers must
replace both values with their own Vendor ID and Vendor Name. See the existing
[BACnet Vendor Identifier guidance](../../README.md#bacnet-vendor-identifier)
and its ASHRAE assignment links.

## Local network configuration

Copy `src/secret/secrets.example.h` to `src/secret/secrets.h` and set local
WiFi credentials there. The real file is ignored by Git. Device identity and
BACnet settings deliberately do not belong in `secrets.h`.

```sh
pio run -d examples/server-demo -e usb
pio run -d examples/server-demo -e eth
```

For the authorized WiFi HIL setup, upload only to COM4:

```powershell
pio run -d examples/server-demo -e usb -t upload --upload-port COM4
pio device monitor --port COM4 --baud 115200
```

The startup log prints the local IP, Device ID, UDP port, and Vendor ID without
printing credentials. The `eth-com6` environment is retained only as a local
Ethernet-client convenience; this demo task does not upload to COM6.

The normal loop has no blocking delay. It refreshes AV200 at the bounded update
rate and polls the server once per loop; incoming Who-Is and ReadProperty
datagrams receive replies at their observed source IP and source port.
