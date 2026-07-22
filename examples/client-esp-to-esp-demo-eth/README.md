# ESP-to-ESP Ethernet BACnet Client Demo

This permanent paired-demo wrapper is the Ethernet BACnet client side of the
ESP-to-ESP COV setup. It reuses the proven ETH client source without changing
it. The checked-in target `192.168.2.126` and Device instance `1682127` are
laboratory configuration values for the paired Wi-Fi server, not generic
BACnet defaults. The wrapper keeps paired-demo runtime diagnostics in its own
`esp2esp_cli` Preferences namespace.

The inherited client discovers the server object list, subscribes supported
process values through COV, renews subscriptions, and restores its client
session after Ethernet reconnects. Existing client GUI cards display the
remote process values; the **ESP-to-ESP** page adds compact paired-demo status
and persistent counters.

The **Server Hardware Inputs** card is populated exclusively from this client's
runtime COV/property cache. It displays the remote AI0 light sensor as a
percentage and BI0 Reset, BI1 Mid, and BI2 Set button states; the server's raw
LDR ADC value is not shown because it is not a transferred BACnet object. Each
row carries an independent receive-mode marker: `C` means an active COV
subscription, `P` means polling fallback, and `?` means pending or unknown
subscription state. The button lamp shows only the remote button value (grey
inactive, green active); it is deliberately independent of the `C`/`P`/`?`
receive-mode marker.

## Build and upload

```sh
pio run -d examples/client-esp-to-esp-demo-eth -e eth-com6
pio run -d examples/client-esp-to-esp-demo-eth -e eth-com6 -t upload
pio device monitor -p COM6 -b 115200
```

The `eth-com6` environment uses the tested WT32-ETH01 DTR/RTS automatic-reset
settings. Open the ConfigManager GUI at the Ethernet address shown on the
serial console; `192.168.2.127` is the current laboratory configuration. See
[Change of Value (COV)](../../docs/cov.md) and the paired
[COV HIL runner](../hil-cov-espClient-to-espServer-acceptance/README.md) for
the protocol and acceptance scope.

## Quiet diagnostic build

`eth-com6-quiet` keeps BACnet and client-demo behavior enabled while compiling
out BACnet logging, client-demo logging, and the shared demo's periodic serial
diagnostics:

```sh
pio run -d examples/client-esp-to-esp-demo-eth -e eth-com6-quiet
```

It does not disable ESP-IDF, Ethernet, PHY, Wi-Fi, or other framework-driver
logs.

## Laboratory serial-reset finding

The paired HIL station was verified with its external DTR/RTS auto-reset
transistor circuit removed and only the manual BOOT and RESET buttons connected.
With that hardware change, the Ethernet client remained stable while the serial
monitor was closed. The earlier apparent serial-closed failure was traced to
the external reset/boot-strapping circuit pulling GPIO0 to an unsafe level and
causing reset or download-mode behavior. It is not a confirmed BACnet, logging,
or closed-UART software failure.
