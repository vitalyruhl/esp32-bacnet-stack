# Planned Server Work

BACnet/IP server support is delivered as a small portable read-only subset, not
as a completed server feature set.

## Current Boundary

- The repository includes a portable `BacnetServer` runtime with configurable
  Device identity, non-blocking `poll()`, Who-Is decoding, I-Am responses,
  ReadProperty responses, and Reject responses for unsupported confirmed
  services.
- The portable object model exposes the Device Object and caller-owned,
  read-only Analog Value objects. `examples/server-demo` injects the existing
  Arduino UDP adapter and keeps WiFi/Ethernet setup and demo binding outside
  those portable modules.
- Analog Values support stored Present Values or a lightweight function-pointer
  provider with caller context. No globally allocated Analog Value capacity is
  reserved when no values are registered.
- The current project focus remains the BACnet/IP client roadmap and validation.
- The demo is intentionally read-only and does not claim COV, alarm, priority,
  WriteProperty, real I/O, or production-server coverage.

## Planning Notes

- Future server work must stay separate from demo-only glue and from optional UI integrations.
- COV, alarms, events, priorities, WriteProperty, trends, real I/O, and
  platform-specific adapters remain separate work.
