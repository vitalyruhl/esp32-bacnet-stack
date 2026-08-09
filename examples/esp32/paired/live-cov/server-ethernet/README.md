# ESP-to-ESP Ethernet BACnet Server Demo

This permanent paired-demo wrapper runs the existing I/O BACnet server and its
live-COV additions on a WT32-ETH01. It keeps the existing WiFi-server /
Ethernet-client pairing unchanged and provides the inverse role combination:
the Ethernet server is Device `1682127` at `192.168.2.126` by default, while
the paired WiFi client targets that endpoint.

The server exposes the same COV-relevant objects as the WiFi server: AI0 light,
AI1 temperature, BI0 Reset, BI1 Mid, BI2 Set, BO0 LED 1, BO1 LED 2, and the
commandable BV320. The **ESP-to-ESP** GUI page retains the BV320 effective
value, priority, active COV-subscription count, and persistent diagnostics in
the independent `esp2esp_srv_eth` Preferences namespace. Ethernet connection
losses and reconnects are counted without sharing NVS state with the WiFi
server profile.

Ethernet address, gateway, subnet, and DNS values are ConfigManager settings.
Their defaults are laboratory values, not deployment defaults. No WiFi secret
is used or stored by this profile.

## Build and upload

```sh
pio run -d examples/esp32/paired/live-cov/server-ethernet -e eth-com6
pio run -d examples/esp32/paired/live-cov/server-ethernet -e eth-com6 -t upload
pio device monitor -p COM6 -b 115200
```

The `eth-com6` environment is a local COM6 convenience profile. Reset the
board manually after flashing when it remains in download mode. Open the
ConfigManager GUI at the Ethernet address shown on the serial console. For the
COV protocol scope, see [Change of Value (COV)](../../../../../docs/cov.md) and
the paired [COV HIL runner](../../../../../tests/hil/esp32/cov-client-server-acceptance/README.md).

## Quiet diagnostic build

```sh
pio run -d examples/esp32/paired/live-cov/server-ethernet -e eth-com6-quiet
```

This only compiles out direct BACnet and COV diagnostics; it does not alter
BACnet, Ethernet, GUI, or COV behavior.
