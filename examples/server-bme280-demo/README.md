# BACnet BME280 Server Demo

This ESP32 WiFi example exposes a BME280 through the portable read-only BACnet
server and persists setup through ConfigManager 4.4.0. It requires no BME280
for compilation; real sensor HIL remains required before merging.

## Wiring and sensor

Use I2C SDA GPIO21 and SCL GPIO22. The BME280 address setting accepts `0x76`
or `0x77` (default `0x76`). The demo keeps the last valid values when a read is
invalid and marks all AVs with `fault=true`, `Event_State=fault`, and
`Reliability=no-sensor`.

## Persistent settings

- BACnet Device Instance — restart required
- BACnet UDP Port — restart required; default `47808`
- AV Base Instance — restart required; default `300`
- BME280 I2C Address — restart required
- Dew Point Warning Minimum/Maximum
- Dew Point Error Minimum/Maximum
- Dew Point Deadband/Hysteresis

The startup validator requires four non-colliding AV instances and
`errorMin <= warningMin < warningMax <= errorMax`. Invalid persisted BACnet or
sensor settings use safe defaults and log an error. Invalid dew-point limits
also fall back to documented defaults for state evaluation.

## BACnet object mapping

With base `300`, the demo exposes AV300 temperature (degrees Celsius), AV301
relative humidity (percent relative humidity), AV302 pressure (pascals), and
AV303 calculated dew point (degrees Celsius). A different base uses the same
four `+0` through `+3` offsets after restart.

Every AV advertises Object Identifier, Object Name, Object Type, Present Value,
Description, Units, Min/Max Present Value, Resolution, Status Flags, Event
State, Out Of Service, Reliability, and Property List. Min/Max are technical
measurement/display bounds, not warning or error thresholds.

The dew point uses the Magnus approximation with `a=17.62` and `b=243.12 °C`.
Non-finite values and humidity outside `(0, 100]` are rejected.

## Dew-point state mapping

Within warnings: normal, no alarm/fault, reliability `no-fault-detected`.
Outside a warning but inside error bounds: Low/High Limit Event_State,
`inAlarm=true`, no fault. Outside error bounds: Low/High Limit Event_State,
`fault=true`, Reliability `under-range`/`over-range`. Hysteresis is applied on
return from the corresponding side. This slice exposes state only; it sends no
BACnet event notifications, COV, or Notification-Class messages.

## Build

```sh
pio run -d examples/server-bme280-demo -e usb
```
