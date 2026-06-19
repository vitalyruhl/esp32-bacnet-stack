# Known Pitfalls

- Status: early project pitfalls summary.
- This memory is orientation only and does not replace repository files or governance.

## Governance / Workflow

- Do not edit `main` or `master` directly except the narrow docs-only exception defined in governance.
- Do not treat Serena memories as canonical governance.
- Do not rely on broad `rg` or `fd` searches for governance; hidden paths may be skipped.
- Do not mark issues solved or fixed until the user confirms that the result works.
- Do not delete branches unless integration into `main` is verified.

## BACnet / Architecture

- Do not treat BACnet/IP and BACnet MS/TP as interchangeable.
- Do not implement ReadProperty, WriteProperty, MS/TP, or a full object model as part of a minimal discovery slice unless explicitly requested.
- Do not import external BACnet stack source code without explicit license, provenance, and architecture review.
- Commercial CAS/Chipkin examples may be useful operational references, but their implementation material is not reusable open-source code.
- Gateway-specific examples must not drive the default core library architecture.

## ESP32 / PlatformIO

- Upload, monitor, erase, OTA, and hardware/network-affecting commands require explicit user approval.
- Do not put secrets, WiFi credentials, serial ports, OTA addresses, or local hardware details into shared Serena memories.
- Example projects use `lib_deps = symlink://../..`; avoid copying local build artifacts into library packages.
- PlatformIO validation should use the commands from the project profile and current repository configuration.

Sources: `.github/AGENTS.md`, `.github/agents/project.agent.md`, `platformio.ini`, `examples/**/platformio.ini`, `library.json`.