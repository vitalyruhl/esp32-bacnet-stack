# ESP-to-ESP WiFi BACnet Server Demo

This permanent paired-demo wrapper reuses the proven `server-io-example`
source without changing it. It adds only paired-demo GUI cards and persistent
runtime diagnostics in the dedicated `esp2esp_srv` Preferences namespace.

The target server is the WiFi ESP32 on COM7. Configure WiFi through
ConfigManager; the paired Ethernet client uses BACnet/IP address
`192.168.2.126` and device instance `1682127` by default.

## Build and upload

```sh
pio run -d examples/server-esp-to-esp-demo-wifi -e wifi-com7
pio run -d examples/server-esp-to-esp-demo-wifi -e wifi-com7 -t upload
pio device monitor -p COM7 -b 115200
```

Open the ConfigManager GUI at the WiFi address shown on the serial console.
The **ESP-to-ESP** page shows local BACnet process objects, COV peer state,
and persistent restart/network diagnostics.
