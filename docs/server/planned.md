# Planned Server Work

BACnet/IP server support is planned but not yet delivered as a completed server feature set.

## Current Boundary

- The repository includes a portable `BacnetServer` runtime with configurable
  Device identity, non-blocking `poll()`, Who-Is decoding, I-Am responses, and
  Reject responses for unsupported confirmed services.
- The runtime has no object/property model, ReadProperty support, platform
  adapter, or ESP32 server demo transport binding.
- The current project focus remains the BACnet/IP client roadmap and validation.
- This page must not be read as evidence of a completed server MVP or usable
  ESP32 server behavior.

## Planning Notes

- Server MVP is gated behind client runtime completion milestones.
- Future server work must stay separate from demo-only glue and from optional UI integrations.
- Future server docs should document implemented behavior only after code and validation exist.
