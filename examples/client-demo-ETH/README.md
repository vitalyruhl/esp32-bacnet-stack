# BACnet Client Demo - Ethernet

This variant runs the full BACnet/IP client demo on a Wireless-Tag WT32-ETH01
V1.4 (WT32-S1) through its LAN8720 Ethernet interface. WiFi is compiled out of
this project. The shared client-demo application remains the same as the WiFi
variant; only the network adapter and ConfigManager network settings differ.

ConfigManager V4.4.0 stores the Ethernet IP, subnet, gateway, DNS, settings
password, and system/NTP settings. Address changes take effect after a reboot.
The tracked first-run default is `192.168.2.127/24`; copy
`src/secret/secrets.example.h` to `src/secret/secrets.h` to use other local
defaults. The tracked WAGO test target uses Device instance `1682101` at
`192.168.2.101:47808`. If the bounded object-list preview fills before a value
 category is stored, the demo probes its configured AV220, BV320, or MSV2020
 reference object for that missing category.

## Monitoring and manual overrides

The monitoring card shows AV200 through SubscribeCOV and AV201 through polling.
SubscribeCOV has no polling fallback; a subscription error remains visible while
AV201 polling continues. The settings page provides the object instances, COV
lifetime, and Write Priority (`1..16`).

The separate **Manual Priority Overrides** card contains a numeric AV input,
AV write/relinquish actions, and BV Set 0, Set 1, and relinquish actions. Each
button sends at most one request and only after an explicit user click. The
Ethernet build enables `ESP_BACNET_ENABLE_WRITE_PROPERTY` and
`ESP_BACNET_ENABLE_PRIORITY_WRITE`; other demo environments retain disabled
write features. A successful override remains active until relinquished.

## Build, upload, and monitor

Run from the repository root:

```sh
pio run -d examples/client-demo-ETH -e eth
pio run -d examples/client-demo-ETH -e eth-com6 -t upload
pio device monitor -d COM6 -b 115200
```

The `eth-com6` environment is a local convenience profile for the current test
adapter. Select the actual serial port on other systems.

## Bootloader wiring

Use 3.3 V UART logic levels with the AZ-Delivery USB-to-serial adapter:

- Adapter TX to board RX0 (GPIO3)
- Adapter RX to board TX0 (GPIO1)
- Adapter GND to board GND
- Hold GPIO0 low while resetting or powering up to enter the bootloader

[WARNING] The ESP32 and Ethernet PHY can exceed the current available from a
small USB-to-serial adapter regulator. Use a stable supply rated for at least
500 mA, share ground, and never drive both board supply inputs. Brownout loops
usually indicate insufficient power.
