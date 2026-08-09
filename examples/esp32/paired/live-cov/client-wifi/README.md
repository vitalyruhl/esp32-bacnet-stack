# ESP-to-ESP WiFi BACnet Client Demo

This permanent paired-demo wrapper uses the established BACnet browser,
subscription, GUI, and diagnostics runtime as a WiFi client. It is the inverse
of the existing Ethernet-client / WiFi-server pair and targets the new Ethernet
server at `192.168.2.126`, Device `1682127`, by default. It has its own
`esp2esp_cli_wifi` Preferences namespace, so its NVS counters do not mix with
the Ethernet client profile.

The client discovers the Ethernet server object list and subscribes to the
same eight process values: AI0, AI1, BI0 through BI2, BO0, BO1, and BV320.
The **ESP-to-ESP** GUI cards show current cached values, receive mode, COV
updates, renewal attempts, local send failures, timeouts, network reconnects,
and peer recovery. The client recreates its session after a target I-Am
timeout, an empty or failed object-list scan, or sustained loss of every
previously active COV subscription. Subscription renewal and polling fallback
remain the proven shared client behavior. The manual BO0 priority/relinquish
HIL action is also available unchanged; it is never run automatically.

Configure WiFi through ConfigManager. Keep real credentials exclusively in the
ignored `src/secret/secrets.h`; without it, configure the WiFi network through
the ConfigManager access point. The checked-in target and discovery broadcast
are laboratory values, not generic BACnet defaults.

## Build and upload

```sh
pio run -d examples/esp32/paired/live-cov/client-wifi -e wifi-com7
pio run -d examples/esp32/paired/live-cov/client-wifi -e wifi-com7 -t upload
pio device monitor -p COM7 -b 115200
```

Open the ConfigManager GUI at the WiFi address shown on the serial console. For
the COV protocol scope, see [Change of Value (COV)](../../../../../docs/cov.md)
and the paired [COV HIL runner](../../../../../tests/hil/esp32/cov-client-server-acceptance/README.md).

## Quiet diagnostic build

```sh
pio run -d examples/esp32/paired/live-cov/client-wifi -e wifi-com7-quiet
```

This keeps BACnet, COV, reconnect, and GUI behavior enabled while compiling out
the shared client's periodic serial diagnostics.
