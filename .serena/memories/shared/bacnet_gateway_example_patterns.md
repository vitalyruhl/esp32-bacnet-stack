# BACnet Gateway Example Patterns

- Status: mixed verified summary.
- This memory is orientation only and does not replace repository files, user instructions, `.github/AGENTS.md`, or `.github/agents/project.agent.md`.

## Gateway-Specific Patterns

- `DoodzProg/ESP32-BMS-Gateway-Multi-Protocol` is a gateway/product example, not a reusable stack baseline.
- BACnet/IP there is part of a Modbus-to-BACnet bridge role, not a generic client/server library surface.
- Gateway-oriented concerns such as web UI, local persistence, product auth, and protocol-translation state must stay separate from the core BACnet library design.

## Reusable Observations

- Production gateways benefit from explicit separation between protocol handlers and product configuration layers.
- Validation against external BACnet tooling such as YABE is a useful product-level acceptance pattern.
- Gateway configuration and translation logic should remain optional, not a dependency of a reusable BACnet stack.

## Primary Sources

- `DoodzProg/ESP32-BMS-Gateway-Multi-Protocol`: `README.md`, `CONTRIBUTING.md`, `src/bacnet_handler.cpp`
