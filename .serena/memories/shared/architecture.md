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

## BACnet Client Object Discovery Boundary

- Keep `BacnetClient` intentionally small and reusable:
  - UDP lifecycle
  - Who-Is / I-Am handling
  - ReadProperty request/response
  - future WriteProperty request/response
  - basic BACnet encoding/decoding
  - logging integration
- Object enumeration, object browsing, and explorer-style result caches must not
  become mandatory `BacnetClient` core behavior.
- BACnet Device Object `object-list` discovery belongs in optional helper or
  demo layers. A future bounded `BacnetObjectDiscovery` or
  `BacnetObjectEnumerator` helper may be added, but minimal clients must still
  be able to read/write known BACnet points without pulling in browsing state.
- Demo GUI scan state must not leak into the core library.
- AV200/MV2000 range probing is fallback/debug behavior only. The primary
  discovery strategy for the client demo is Device Object `object-list`.

### WAGO Object-List Validation Facts

- Local validation target: WAGO BACnet device ID `<DEVICE_INSTANCE>`, vendor 222, model
  `750-8212/0000-0100`.
- Runtime Device Object `object-list` count observed: 544.
- The local ignored validation export also contains 544 BACnet objects. The
  export is local validation input only and must not be committed or copied
  wholesale into tracked documentation.
- Relevant object distribution from the export:
  - `ANALOG_INPUT`: 22 objects; first `AI0`; object-list starts around position
    1.
  - `ANALOG_OUTPUT`: 11 objects; first `AO0`; object-list starts around position
    23.
  - `ANALOG_VALUE`: 160 objects; first `AV200` /
    `TTT_ANALOG_VALUE_200`; object-list starts around position 34.
  - `BINARY_VALUE`: 160 objects; first `BV0`; object-list starts around
    position 298.
  - `MULTISTATE_VALUE`: 24 objects; first `MV2000` /
    `TTT_MULTISTATE_VALUE_2000`; object-list starts around position 487.
- Consequences for the demo scanner:
  - `maxObjectList=100` can reach AV objects but cannot reach MV objects on
    this target.
  - The demo default should inspect the full observed WAGO object-list while
    remaining bounded, for example `maxObjectList=600`.
  - The effective enumeration limit is `min(objectListCount,
    configuredMaxObjectList)`.
  - AV and MV discovery must be tracked independently. The demo must not stop
    after filling the first 10 AV display rows if MV objects are still later in
    the list.

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
