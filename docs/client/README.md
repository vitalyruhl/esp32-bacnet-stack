# Client Guide

This section is the entry point for the current BACnet/IP client documentation.

Use the linked pages depending on what you need:

- [Important Notes](important.md) for lifecycle, polling, scan, subscription, and safety boundaries.
- [Client API](api.md) for the current public API overview.
- [Client Examples](examples.md) for the committed example projects and what each one demonstrates.

## Current Scope

The implemented client scope is read-oriented BACnet/IP work on ESP32:

- Who-Is / I-Am discovery
- known-device sessions through `BacnetDeviceSession`
- ReadProperty access for known objects and properties
- object-list scanning in blocking and non-blocking forms
- property-list discovery and safe read-all attempts with per-property status
- property subscription abstraction with fallback polling

The client documentation describes implemented behavior only. Planned items such as SubscribeCOV and writes are documented as planned, not as available features.
