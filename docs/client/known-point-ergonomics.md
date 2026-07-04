# Known-Point Ergonomics Direction

This page records the intended direction for small ESP32 BACnet client programs
that already know the BACnet points they need. It is a planning note, not a
description of a fully implemented high-level API.

## Target Use Case

A typical small client reads a fixed set of about 10 BACnet points or fewer.
For example, an application may read `AV200`, check present value and object
health, compare the numeric value against configured limits, and then publish a
result through MQTT or local application logic.

The application should not need to know the BACnet property numbers for
present-value, status-flags, event-state, reliability, out-of-service, units,
or range metadata for that common flow.

## Current Building Blocks

The current library already provides the lower-level pieces needed for this
direction:

- `BacnetDeviceSession` for known device endpoints.
- `BacnetProcessObject` helpers such as `analogValue()` and `readStatus()`.
- `BacnetPropertySubscription` with fallback polling.
- `BacnetValue` storage for typed values and display text.
- Reusable display, scanned-object, numeric range, and status predicate helpers.

## Near-Term Shape

Prefer small helpers before introducing a larger client facade. A future
known-point API should remain fixed-capacity friendly and avoid hidden heap
growth. A possible shape is:

```cpp
BacnetKnownPoint av200(BacnetObjectType::AnalogValue, 200);

if (av200.isNormal() && av200.valueInRange(minValue, maxValue)) {
  publishValue(av200.presentValue());
}
```

The first implementation step should only add types or helpers that remove
real duplication from examples or user code. Avoid a large `BacnetSimpleClient`
architecture until the small helper layer is proven.

## Out Of Scope For This Phase

- BACnet write API implementation.
- MQTT or Home Assistant adapters in the BACnet library.
- Reintroducing BME/BME280/BME350 sensor logic into `examples/client-demo`.
- Moving ConfigManager-specific UI code into the BACnet library.

Write support should be designed as a separate layer so read/cache/status code
can stay small on constrained ESP32 builds.