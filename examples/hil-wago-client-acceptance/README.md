# WAGO Client Acceptance HIL Runner

Local-only hardware-in-the-loop acceptance runner for ESP32 BACnet client
runtime behavior against a real WAGO BACnet/IP target.

This runner is intentionally consolidated. Add future client scenarios here
instead of creating many small one-feature HIL examples.

## Scope

- Not required by normal CI.
- Requires local ESP32 hardware on USB.
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

Current planned scenario blocks (runner scaffold exists):

- property cache
- subscribe-any-property
- fallback polling value change
- SubscribeCOV where supported
- WriteProperty only when explicitly enabled
- PresentValue priority write only when explicitly enabled

Writes are disabled by default. Write scenarios must remain explicit opt-in via
local secrets flags.

`S02` is enabled by default in the local secrets template and reads
`present-value` from configured AI/AO/AV, BI/BO/BV, MI/MO/MV objects without
BACnet writes. Required objects fail the scenario when unreadable or decoded
with an unexpected value type. Optional objects can be set to `0` in
`src/secret/secrets.h` so they are skipped.

## Build And Run

Build without upload:

```sh
pio run -d examples/hil-wago-client-acceptance -e usb
```

Run locally:

```sh
pio run -d examples/hil-wago-client-acceptance -e usb -t upload
pio device monitor -d examples/hil-wago-client-acceptance -e usb
```

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
