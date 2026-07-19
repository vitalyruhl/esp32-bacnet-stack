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
- The server implements a bounded COV subset: object-level Present_Value plus
  Status_Flags and property-level subscriptions, including confirmed delivery,
  renewal, expiry, cancellation, and real-valued COV_Increment. It does not
  claim alarm/event reporting, Analog Output/PWM, relay safety policies, or
  production-server coverage.

## Planning Notes

- Future server work must stay separate from demo-only glue and from optional UI integrations.
- Alarms, events, Analog Output/PWM, relay failsafes, trends, additional real
  I/O, and platform-specific adapters remain separate work. COV extensions
  beyond the implemented bounded subset also remain separate work.
