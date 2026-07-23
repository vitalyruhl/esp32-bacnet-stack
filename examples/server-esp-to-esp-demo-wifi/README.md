# ESP-to-ESP WiFi BACnet Server Demo

This permanent paired-demo wrapper is the Wi-Fi BACnet server side of the
ESP-to-ESP COV setup. It reuses the proven `server-io-example` source and adds
the paired-demo-only commandable Binary Value `BV320`, GUI cards, and
persistent runtime diagnostics in the dedicated `esp2esp_srv` Preferences
namespace. `BV320` is a BACnet Binary Value object, not a hardware output.

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
The **ESP-to-ESP** page shows BV320's effective value, effective priority, and
active Present_Value COV-subscription count alongside local BACnet process
objects, COV peer state, and persistent restart/network diagnostics. `BV320`
accepts WriteProperty priorities `1..16`; writing BACnet `NULL` relinquishes
the selected slot and can expose the next priority or `Relinquish_Default`.
For the COV protocol scope, see
[Change of Value (COV)](../../docs/cov.md); for the paired acceptance scenario,
see the [COV HIL runner](../hil-cov-espClient-to-espServer-acceptance/README.md).

## WAGO BACnet Configurator check

Find **Binary Value 320** on Device `1682127`, then write `Present_Value` in
this order: `ACTIVE` at priority `16`, `INACTIVE` at priority `8`, BACnet
`NULL` at priority `8`, and BACnet `NULL` at priority `16`. The effective value
must respectively be active, inactive, active, and the false
`Relinquish_Default`. The paired client should show a BV320 COV update only for
those four effective-value changes; changing a hidden lower-priority slot must
not create a value update.

## Quiet diagnostic build

`wifi-com7-quiet` compiles out BACnet logging and the wrapper's direct COV
diagnostics without changing BACnet behavior:

```sh
pio run -d examples/server-esp-to-esp-demo-wifi -e wifi-com7-quiet
```

It does not disable ESP-IDF, Wi-Fi, PHY, or other framework-driver logs.
