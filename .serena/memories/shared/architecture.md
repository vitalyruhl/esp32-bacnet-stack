# Architecture

- Status: early scaffold summary.
- This memory is orientation only and does not replace repository files or governance.
- The library is intended as a reusable ESP32/Arduino BACnet stack.
- The current public direction separates two roles:
  - `BacnetClient`
  - `BacnetServer`
- BACnet/IP is the first implementation target.
- BACnet MS/TP is planned later and must not be assumed implemented.
- Keep reusable BACnet stack code separate from:
  - examples
  - tests
  - generated artifacts
  - repository tooling
- Keep protocol encoding and decoding boundaries explicit.
- Do not import external BACnet implementation code without explicit license, provenance, and architecture review.
- Do not add core dependencies on optional demo integrations.
- ESP32 Configuration Manager is not a core dependency.

## Current MVP Direction

- Client MVP should start with minimal BACnet/IP discovery:
  - Who-Is request building
  - I-Am response parsing
  - no ReadProperty yet
  - no WriteProperty yet
  - no MS/TP yet
  - no full object model yet
- Server MVP should stay separate from client implementation and define minimal device/object behavior only when explicitly scoped.

Sources: `src/EspBacnet.h`, `src/BacnetClient.h`, `src/BacnetServer.h`, `README.md`, `.github/agents/project.agent.md`.