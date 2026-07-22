# ESP-to-ESP WiFi BACnet Server Demo

This permanent paired-demo wrapper is the Wi-Fi BACnet server side of the
ESP-to-ESP COV setup. It reuses the proven `server-io-example` source without
changing it, and adds only paired-demo GUI cards and persistent runtime
diagnostics in the dedicated `esp2esp_srv` Preferences namespace.

Configure Wi-Fi through ConfigManager. The checked-in `wifi-com7` environment
is the local laboratory convenience profile. Its paired client target
`192.168.2.126` and Device instance `1682127` are lab configuration values,
not general defaults for a deployed BACnet network.

## Build and upload

```sh
pio run -d examples/server-esp-to-esp-demo-wifi -e wifi-com7
pio run -d examples/server-esp-to-esp-demo-wifi -e wifi-com7 -t upload
pio device monitor -p COM7 -b 115200
```

Open the ConfigManager GUI at the WiFi address shown on the serial console.
The **ESP-to-ESP** page shows local BACnet process objects, COV peer state,
and persistent restart/network diagnostics. For the COV protocol scope, see
[Change of Value (COV)](../../docs/cov.md); for the paired acceptance scenario,
see the [COV HIL runner](../hil-cov-espClient-to-espServer-acceptance/README.md).

## Quiet diagnostic build

`wifi-com7-quiet` compiles out BACnet logging and the wrapper's direct COV
diagnostics without changing BACnet behavior:

```sh
pio run -d examples/server-esp-to-esp-demo-wifi -e wifi-com7-quiet
```

It does not disable ESP-IDF, Wi-Fi, PHY, or other framework-driver logs.
