# BACnet Server Demo

This demo runs the portable read-only `BacnetServer` through the existing
Arduino UDP adapter. The `usb` environment connects an ESP32 through WiFi;
the `eth` environment compiles the same Device and Analog Value profile for a
WT32-ETH01 Ethernet server. Network initialization is separate, while the
BACnet object configuration is shared.

The demo listens on BACnet/IP UDP port `47808` and exposes Device `1682127`:

- AV200 is a stored sine value in percent. It uses offset `20`,
  amplitude `10`, period `60 s`, and is updated at most every `250 ms` through
  the normal server object model.
- AV201 is a polling/callback value in seconds. Its provider returns elapsed
  uptime and does not keep a second stored Present Value copy.

## BACnet server configuration

`BacnetServer::begin(device, localPort)` is the single server-side UDP bind
configuration. Omitting `localPort` binds the transport to
`BacnetServer::kDefaultPort` (`47808`); passing a value binds that local UDP
port instead. The demo passes `kBacnetPort`, which defaults to `47808` and is
the only place to change the demo's BACnet/IP port.

`BacnetServerDevice` holds Device object properties only; it deliberately has
no transport or port field. The demo initializes every currently available
field with an adjacent comment. `applicationSoftwareVersion` is optional: a
null or empty value makes the server expose `firmwareRevision` for that
property, so this demo has one version source (`kDemoVersion`).

Each caller-owned `BacnetServerAnalogValue` entry similarly documents its
instance, object name, stored value, units, Out_Of_Service state, optional
Present_Value provider, and optional provider context directly in `main.cpp`.

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
