# Planned Server Work

BACnet/IP server support is delivered as a small portable subset, not as a
completed server feature set.

## Current Boundary

- The repository includes a portable `BacnetServer` runtime with configurable
  Device identity, non-blocking `poll()`, Who-Is decoding, I-Am responses,
  ReadProperty responses, and Reject responses for unsupported confirmed
  services.
- The portable object model exposes caller-owned read-only inputs/values and
  commandable Binary Outputs. Binary Outputs share a portable 16-slot priority
  state and accept WriteProperty for Present Value only.
- Analog Values support stored Present Values or a lightweight function-pointer
  provider with caller context. No globally allocated Analog Value capacity is
  reserved when no values are registered.
- The current project focus remains the BACnet/IP client roadmap and validation.
- The demo does not claim COV, alarm/event reporting, Analog Output/PWM,
  relay safety policies, or production-server coverage.

## Planning Notes

- Future server work must stay separate from demo-only glue and from optional UI integrations.
- COV, alarms, events, Analog Output/PWM, relay failsafes, trends, additional
  real I/O, and platform-specific adapters remain separate work.
