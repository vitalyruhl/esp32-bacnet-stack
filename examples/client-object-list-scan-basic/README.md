# Canonical Basic BACnet/IP Client Example

This is the serial-only starter example for the reusable BACnet/IP client APIs.
It keeps configuration local, uses no ConfigManager or web UI, and does not
perform BACnet writes.

The sketch demonstrates:

- WiFi startup with optional static IP settings
- BACnet/IP client startup
- known-target `BacnetDeviceSession::fromEndpoint()` creation
- one simple Device `object-name` read through `BacnetProperty`
- object-list scan through `BacnetDeviceSession::scanObjectList()`
- one fallback-polled `present-value` subscription

## Local Configuration

Copy the example secrets template and edit only the ignored local copy:

```sh
cp examples/client-object-list-scan-basic/src/secret/secrets.example.h \
  examples/client-object-list-scan-basic/src/secret/secrets.h
```

Set WiFi credentials, optional static IP values, and the known BACnet/IP target
in `src/secret/secrets.h`. Do not commit real secrets.

The template uses documentation-range IP addresses and a placeholder device
instance. Replace them only in the ignored local `secrets.h` file.

## Build

```sh
pio run -d examples/client-object-list-scan-basic -e usb
```

Upload and serial monitor are local hardware actions and should be run only
when explicitly intended.
