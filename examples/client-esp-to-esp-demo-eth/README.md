# ESP-to-ESP Ethernet BACnet Client Demo

This permanent paired-demo wrapper reuses the proven ETH client source without
changing it. It is configured for the WiFi ESP32 BACnet server at
`192.168.2.126`, device instance `1682127`, and adds paired-demo runtime
diagnostics in its own `esp2esp_cli` Preferences namespace.

The inherited client discovers the server object list, subscribes supported
process values through COV, renews subscriptions, and restores its client
session after Ethernet reconnects. Existing client GUI cards display the
remote process values; the **ESP-to-ESP** page adds compact paired-demo status
and persistent counters.

## Build and upload

```sh
pio run -d examples/client-esp-to-esp-demo-eth -e eth-com6
pio run -d examples/client-esp-to-esp-demo-eth -e eth-com6 -t upload
pio device monitor -p COM6 -b 115200
```

The `eth-com6` environment uses the tested WT32-ETH01 DTR/RTS automatic-reset
settings. Open the ConfigManager GUI at the Ethernet address shown on the
serial console (normally `192.168.2.127`).
