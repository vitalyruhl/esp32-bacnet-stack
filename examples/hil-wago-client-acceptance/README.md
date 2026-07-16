# WAGO Client Acceptance HIL Runner

Local-only hardware-in-the-loop acceptance runner for ESP32 BACnet client
runtime behavior against a real WAGO BACnet/IP target.

This runner is intentionally consolidated. Add future client scenarios here
instead of creating many small one-feature HIL examples.

## Scope

- Not required by normal CI.
- Requires local ESP32 hardware through WiFi or WT32-ETH01 Ethernet.
- Requires reachable WAGO BACnet/IP target.
- Requires local secrets in `src/secret/secrets.h`.

If local secrets are missing, firmware prints:

- `[SKIP] local HIL secrets missing`

and ends with a summary.

`src/secret/secrets.h` is ignored by Git. Commit only
`src/secret/secrets.example.h`.

## Scenarios

Current implemented scenario:

- `S01` non-blocking object-list scan (issue #32 validation)
- `S02` common process object `present-value` reads (local read-only validation)
- `S03` common process object status reads (local read-only validation)

Current planned scenario blocks (runner scaffold exists):

- `S04` property cache
- `S05` subscribe-any-property
- `S06` fallback polling value change
- `S07` SubscribeCOV where supported
- `S08` WriteProperty only when explicitly enabled
- `S09` PresentValue priority write only when explicitly enabled

Writes are disabled by default. Write scenarios must remain explicit opt-in via
local secrets flags.

`S09` uses an explicit priority-8 override and relinquish flow. Its reset
stage uses the writable command-priority reset mode: it relinquishes priorities
1..5 and 7..16, explicitly reports skipped priority 6, and never writes
`priority-array` directly. The Strict and writable reset semantics, including
the WAGO compatibility observation, are documented in
[`docs/bacnet-command-priority.md`](../../docs/bacnet-command-priority.md).

`S02` is enabled by default in the local secrets template and reads
`present-value` from configured AI/AO/AV, BI/BO/BV, MI/MO/MV objects without
BACnet writes. Required objects fail the scenario when unreadable or decoded
with an unexpected value type. Optional objects can be set to `0` in
`src/secret/secrets.h` so they are skipped.

`S03` is enabled by default in the local secrets template and reads
`status-flags`, `event-state`, `reliability`, and `out-of-service` where
available. Unsupported status properties are reported per property and do not
abort the scenario. The derived object state is conservative and read-only.

## Build And Run

Build without upload:

```sh
pio run -d examples/hil-wago-client-acceptance -e usb
pio run -d examples/hil-wago-client-acceptance -e eth
```

Run locally:

```sh
pio run -d examples/hil-wago-client-acceptance -e usb -t upload
pio device monitor -b 115200
```

For the current WT32-ETH01/AZ-Delivery adapter setup, use `-e eth-com6` for
upload and monitor COM6 at 115200 baud. Other systems must select their actual
port. Hold GPIO0 low during reset for the bootloader and provide enough current
for both the ESP32 and Ethernet PHY.

## Output Contract

- Compact scenario lines with `PASS`, `FAIL`, or `SKIP`
- Final summary line with `total/pass/fail/skip`
- Final result is `PASS` only when all required enabled scenarios pass

## How To Extend

1. Add a new scenario function in `src/main.cpp`.
2. Register it in `runAcceptanceRunner()` with scenario ID/name.
3. Mark required vs optional explicitly.
4. Gate risky actions behind local opt-in flags in `src/secret/secrets.h`.
5. Keep local writes disabled by default.
