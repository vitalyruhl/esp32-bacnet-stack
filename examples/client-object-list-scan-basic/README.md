# Canonical Basic BACnet/IP Client Example

This is the serial-only starter example for the reusable BACnet/IP client APIs.
It keeps configuration local, uses no ConfigManager or web UI, and does not
perform BACnet writes.

The sketch demonstrates:

- WiFi or WT32-ETH01 Ethernet startup with optional static IP settings
- BACnet/IP client startup
- known-target `BacnetDeviceSession::fromEndpoint()` creation
- one simple Device `object-name` read through `BacnetProperty`
- object-list scan through `BacnetDeviceSession::scanObjectList()` limited to
  common AI/AO/AV, BI/BO/BV, and MI/MO/MSV process objects
- one fallback-polled `present-value` subscription chosen from scanned process
  objects

The example remains intentionally read-only. It demonstrates the reusable
client/session/object APIs without ConfigManager, web UI, or BACnet writes.

## Local Configuration

Copy the example secrets template and edit only the ignored local copy:

```sh
cp examples/client-object-list-scan-basic/src/secret/secrets.example.h \
  examples/client-object-list-scan-basic/src/secret/secrets.h
```

Set WiFi credentials, optional static IP values, and the known BACnet/IP target
in `src/secret/secrets.h`. The Ethernet environment reuses the tracked
`MY_WIFI_IP` value as its local wired address unless `MY_ETHERNET_IP` is defined
in the ignored local file. Do not commit real secrets.

The template uses documentation-range IP addresses and a placeholder device
instance. Replace them only in the ignored local `secrets.h` file.

## Build

```sh
pio run -d examples/client-object-list-scan-basic -e usb
pio run -d examples/client-object-list-scan-basic -e eth
```

The optional `eth-com6` environment adds the current local upload/monitor port.
COM6 is not assumed on other systems. Hold GPIO0 low during reset to enter the
WT32-ETH01 bootloader and use a stable supply for the ESP32 and Ethernet PHY.

WiFi startup uses the reusable ESP32-only
`examples/common/Esp32WiFiNetwork.h` helper. Transport and BACnet session
creation remain directly visible in the example.
