# ESP32 BACnet Porting Candidates

- Status: mixed verified reference summary.
- This memory is orientation only and does not replace repository files, user instructions, `.github/AGENTS.md`, or `.github/agents/project.agent.md`.

## Current Repo Facts

- The current repo targets ESP32 with PlatformIO and Arduino.
- The default validation environment is `usb`.
- BACnet/IP is the current implementation target; MS/TP is planned later and must not be assumed implemented here.

## Porting Candidates

- `bacnet-stack/bacnet-stack`
  - Verified ESP32 port area exists under `ports/esp32/`.
  - Verified BACnet/IP and MS/TP evidence exists upstream.
  - Treat as protocol and datalink reference, not Arduino-ready drop-in code.
- `KaiDroste/bacnet-stack-esp32`
  - Verified ESP32-focused fork/port with separate BACnet/IP and MS/TP port areas.
  - Useful for ESP-IDF porting structure and datalink wiring patterns.
  - Maintenance freshness is unverified.
- `MGuerrero31416/BACnet-ESP32-Display`
  - Verified production-style ESP32 firmware example with WiFi, RS-485, display, and NVS usage.
  - Useful for hardware integration and multitask firmware patterns.
  - Not a reusable library baseline.

## Non-candidates For Core Reuse

- `chipkin/ESP32-BACnetServerExample`: proprietary stack example; do not treat as reusable open-source implementation material.
- `DoodzProg/ESP32-BMS-Gateway-Multi-Protocol`: gateway/product example; useful for product lessons, not for default core-library structure.

## Porting Reminders

- Keep UDP and RS-485 datalink boundaries explicit.
- Document GPIO, UART, and transceiver-control pins explicitly when borrowing firmware patterns.
- Separate repo-local validation commands from upstream/reference build procedures.

## Primary Sources

- `platformio.ini`, `.github/agents/project.agent.md`
- `bacnet-stack/bacnet-stack`: `ports/esp32/README.md`
- `KaiDroste/bacnet-stack-esp32`: `ports/esp32/`, `ports/esp32_mstp/README.md`
- `MGuerrero31416/BACnet-ESP32-Display`: `README.md`, `main/main.c`
