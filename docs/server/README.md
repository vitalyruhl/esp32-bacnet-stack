# Server Guide

The portable `BacnetServer` runtime accepts an injected
`BacnetDatagramTransport`, is driven by non-blocking `poll()`, decodes Who-Is,
emits I-Am, and serves read-only Device and registered Analog Value objects
through ReadProperty.

The active Device profile exposes its mandatory identity, protocol-capability,
and transport properties, an Object List, Property List, and an empty Device
Address Binding list. It returns BACnet errors for unknown objects/properties
and invalid array indices. Object and property lists support full, count
(`index 0`), and individual-entry reads.

Analog Values are supplied in caller-owned `BacnetServerAnalogValue` storage
through `setAnalogValues()`. Registering no entries consumes no object-table
storage in the server and does not advertise Analog Value support. Each entry
provides Object Identifier, Object Name, Object Type, Present Value, Status
Flags, Event State, Out Of Service, Units, and Property List. Status Flags only
reports the implemented Out Of Service bit, and Event State is always normal.

The Present Value is read from the entry's stored `presentValue` when no
provider is configured. A configured `BacnetServerAnalogValueProvider` is a
function pointer plus caller context and is invoked only while serving a
Present Value read. The server neither owns the entries, strings, nor provider
context; all must remain valid while the server is running.

The runtime borrows the transport; the caller keeps that transport alive for
the server lifetime and does not share it between running server instances. It
does not allocate from the heap in its normal request path and intentionally
does not store a clock or logger. Later features that need either must use the
existing portable clock and logging abstractions.

This is not a complete BACnet/IP server feature: there is no COV, alarm,
priority, WriteProperty, real I/O, ESP32 network adapter, or server demo
transport binding.

See [Planned Server Work](planned.md) for the current scope boundary.
