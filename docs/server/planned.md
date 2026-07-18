# Planned Server Work

BACnet/IP server support is delivered as a small portable read-only subset, not
as a completed server feature set.

## Current Boundary

- The repository includes a portable `BacnetServer` runtime with configurable
  Device identity, non-blocking `poll()`, Who-Is decoding, I-Am responses,
  ReadProperty responses, and Reject responses for unsupported confirmed
  services.
- The portable object model exposes the Device Object and caller-owned,
  read-only Analog Value objects. It has no platform adapter or ESP32 server
  demo transport binding.
- Analog Values support stored Present Values or a lightweight function-pointer
  provider with caller context. No globally allocated Analog Value capacity is
  reserved when no values are registered.
- The current project focus remains the BACnet/IP client roadmap and validation.
- This page must not be read as evidence of usable ESP32 server behavior.

## Planning Notes

- Future server work must stay separate from demo-only glue and from optional UI integrations.
- COV, alarms, events, priorities, WriteProperty, trends, real I/O, and
  platform-specific adapters remain separate work.
