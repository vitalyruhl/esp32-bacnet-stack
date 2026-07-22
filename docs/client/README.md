# Client Guide

This section is the entry point for the current BACnet/IP client documentation.

Use the linked pages depending on what you need:

- [Important Notes](important.md) for lifecycle, polling, scan, subscription, and safety boundaries.
- [Client API](api.md) for the current public API overview.
- [Change of Value (COV)](../cov.md) for SubscribeCOV, SubscribeCOVProperty,
  confirmation, lifetime, renewal, cancellation, and polling-fallback scope.
- [Client Examples](examples.md) for the committed example projects and what each one demonstrates.
- [Known-Point Ergonomics Direction](known-point-ergonomics.md) for the planned small-client helper direction.

## Current Scope

The implemented client scope is portable BACnet/IP work shared by ESP32
adapters and native Windows applications:

- Who-Is / I-Am discovery
- known-device sessions through `BacnetDeviceSession`
- ReadProperty access for known objects and properties
- object-list scanning in blocking and non-blocking forms
- property-list discovery and safe read-all attempts with per-property status
- property subscription abstraction with fallback polling
- SubscribeCOV and SubscribeCOVProperty registration, renewal, notification
  routing, and polling fallback
- explicit compile-time opt-in WriteProperty for supported `BacnetValue` types
  and command-priority helpers

The portable protocol and client code are shared across platforms. Only
transport, clock, logging, and application integration belong to the Arduino
or Windows adapters. The client documentation describes implemented behavior
only; hardware write validation remains local and explicit. See
[Command priority reset semantics](../bacnet-command-priority.md) for priority
write and reset behavior.
