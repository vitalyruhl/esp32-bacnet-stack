# BACnet Reference Landscape

- Status: mixed verified reference summary.
- This memory is orientation only and does not replace repository files, user instructions, `.github/AGENTS.md`, or `.github/agents/project.agent.md`.
- Current repository role: `esp32-bacnet-stack` is a C++17 Arduino/ESP32 BACnet/IP library with separate `BacnetClient` and `BacnetServer` roles.

## Reference Roles

- `bacnet-stack/bacnet-stack`: upstream BACnet C stack reference with verified BACnet/IP and MS/TP support; reuse requires license and architecture review.
- `KaiDroste/bacnet-stack-esp32`: ESP32-oriented bacnet-stack port/fork with BACnet/IP and MS/TP evidence; maintenance state remains unverified.
- `MGuerrero31416/BACnet-ESP32-Display`: full ESP32 firmware example with BACnet/IP, MS/TP, display, objects, and persistence; useful as firmware-pattern evidence, not as the default library architecture.
- `chipkin/ESP32-BACnetServerExample`: BACnet/IP example around a proprietary CAS/Chipkin stack; operational reference only, not reusable open-source implementation material.
- `DoodzProg/ESP32-BMS-Gateway-Multi-Protocol`: ESP32-S3 Modbus-to-BACnet/IP gateway example; useful gateway/product pattern reference, not a core library architecture template.

## Separation Rules

- Keep reusable open-source implementation references separate from proprietary or commercial-stack references.
- Keep gateway-specific patterns separate from reusable library design.
- Keep user-provided assessments marked unverified until backed by repository sources.

## Primary Sources

- Current repo: `README.md`, `src/EspBacnet.h`, `.github/agents/project.agent.md`
- `bacnet-stack/bacnet-stack`: `README.md`, `ports/esp32/README.md`, `license/readme.txt`
- `KaiDroste/bacnet-stack-esp32`: `ports/esp32/`, `ports/esp32_mstp/README.md`
- `MGuerrero31416/BACnet-ESP32-Display`: `README.md`, `main/main.c`, `LICENSE`
- `chipkin/ESP32-BACnetServerExample`: `README.md`, `src/main.cpp`
- `DoodzProg/ESP32-BMS-Gateway-Multi-Protocol`: `README.md`, `CONTRIBUTING.md`, `src/bacnet_handler.cpp`
