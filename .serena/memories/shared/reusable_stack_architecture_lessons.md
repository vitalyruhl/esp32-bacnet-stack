# Reusable Stack Architecture Lessons

- Status: mixed verified summary.
- This memory is orientation only and does not replace repository files, user instructions, `.github/AGENTS.md`, or `.github/agents/project.agent.md`.

## Stable Lessons

- Keep BACnet datalink handling separate from higher-level client/server roles.
- Keep protocol encoding and decoding boundaries explicit.
- Keep reusable stack code separate from examples, tests, generated artifacts, UI glue, and gateway logic.
- Keep object/device model helpers small enough that known-target reads do not require explorer-style global state.
- Treat discovery results, sessions, and object browsing as separable layers.
- Document transport-specific assumptions explicitly instead of inferring them from object/property code.

## ESP32-Facing Lessons

- Firmware examples that combine WiFi, RS-485, display, or persistence are useful for integration patterns, but they must not define the default library architecture.
- GPIO, UART, transceiver, and storage wiring details belong to examples or adapter layers, not the reusable core library.
- FreeRTOS or multitask usage patterns may inform wrappers, but the core protocol layer should remain portable and conservative.

## Import Constraints

- Do not import external BACnet implementation code without explicit license, provenance, and architecture review.
- Do not treat proprietary stack examples as reusable implementation sources.

## Primary Sources

- Current repo: `src/BacnetClient.h`, `src/BacnetDeviceSession.h`, `src/BacnetServer.h`, `README.md`
- Reference repos: `bacnet-stack/bacnet-stack`, `MGuerrero31416/BACnet-ESP32-Display`, `DoodzProg/ESP32-BMS-Gateway-Multi-Protocol`
