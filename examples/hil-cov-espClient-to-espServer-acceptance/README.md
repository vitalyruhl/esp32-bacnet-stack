# ESP32 COV Acceptance HIL Runner

Local hardware-in-the-loop acceptance runner for BACnet COV between an
Ethernet ESP32 client and a WiFi ESP32 BACnet server. This is separate from
the WAGO client acceptance example.

## Scope

- ETH client: WT32-ETH01 on COM6.
- WiFi server: the I/O example, currently configured for `192.168.2.126` and
  Device instance `1682127`.
- Requires local Ethernet settings in `src/secret/secrets.h`; that file is
  ignored and is never committed.
- The runner sends no ReadProperty request to observe a COV change.

The `eth-com6-cov-hil` environment performs only the following COV checks:

- unconfirmed object-level SubscribeCOV for BI2 Set Button, including initial
  Present_Value and Status_Flags;
- renewal and cancellation of that subscription, with a quiet interval after
  cancellation;
- confirmed object-level SubscribeCOV for BI1 Mid Button, with automatic
  SimpleACK for both notifications;
- SubscribeCOVProperty for BI0 Reset Button Present_Value only;
- SubscribeCOVProperty for AI0 Light Sensor Present_Value with COV_Increment
  `0.5`.

The serial output logs the target, process identifier, subscription type,
confirmation mode, and the failing scenario phase. The operator is prompted
to press each button or change the LDR during a bounded observation window.

## Build and upload

Build only:

```sh
pio run -d examples/hil-cov-espClient-to-espServer-acceptance -e eth-com6-cov-hil
```

For upload, manually place the ETH ESP32 in bootloader mode first:

```sh
pio run -d examples/hil-cov-espClient-to-espServer-acceptance -e eth-com6-cov-hil -t upload
```

After upload, reset the ETH ESP32 normally and monitor COM6 at 115200 baud.
The profile deliberately uses `no_reset` during upload; it cannot put the
board into bootloader mode automatically.

## Result contract

The final result is `PASS` only when the enabled COV scenario completes.
Server-side ConfigManager diagnostics must be checked concurrently for the
single object-level entry, endpoint, process ID, lifetime, notification state,
last send, and confirmed-notification acknowledgement.
