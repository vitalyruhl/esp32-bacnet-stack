# Server Guide

The portable `BacnetServer` runtime accepts an injected
`BacnetDatagramTransport`, is driven by non-blocking `poll()`, decodes Who-Is,
and emits I-Am or a Reject for a complete unsupported confirmed request.

The runtime borrows the transport; the caller keeps that transport alive for
the server lifetime and does not share it between running server instances.
The current stateless MVP has no timeout, scheduling, or
diagnostic requirement, so it intentionally does not store a clock or logger.
Later features that need either must use the existing portable clock and
logging abstractions.

This is not a complete BACnet/IP server feature: there is no object/property
model, ReadProperty, ESP32 network adapter, or server demo transport binding.

See [Planned Server Work](planned.md) for the current scope boundary.
