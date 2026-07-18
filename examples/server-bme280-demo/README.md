# BACnet BME280 Server Demo

This ESP32 WiFi example exposes a BME280 through the portable read-only BACnet
server and persists setup through ConfigManager 4.4.1. It requires no BME280
for compilation.

## Hardware interoperability validation

Real ESP32 WiFi HIL passed on 2026-07-18: the BME280 produced real values, all
four AV300–AV303 objects were read over BACnet, and the ConfigManager Live View
matched BACnet Present_Value exactly. Home Assistant recognized the Device and
all four sensors. Pressure is published in Pascals, including the BME280 hPa to
Pa conversion. Sensor-fault/recovery manipulation, full Intrinsic Reporting,
COV, and WriteProperty/Priority Array remain outside this demo.

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

The demo attaches ConfigManager Core WiFi, System, and NTP settings before it
loads these values. On a first start with no stored SSID, an ignored local
`src/secret/secrets.h` may seed WiFi, static-IP, gateway, subnet, DNS, and
DHCP values once; it saves them and performs one controlled restart. Later
starts never overwrite persisted WiFi settings from that file. Copy the tracked
`secrets.example.h` to create the local file.

The startup validator requires four non-colliding AV instances and
`errorMin <= warningMin < warningMax <= errorMax`. Invalid persisted BACnet or
sensor settings use safe defaults and log an error. Invalid dew-point limits
also fall back to documented defaults for state evaluation.

BACnet objects and the BME280 are configured from the loaded settings once at
startup. The UDP socket is bound exactly once in the ConfigManager WiFi
connected hook, after a station connection exists. A disconnect closes the
socket; a subsequent connected hook may bind it again without re-registering
objects. The loop continues to service ConfigManager, the non-blocking sensor
interval, and BACnet polling without a second WiFi initialization.

## BACnet object mapping

With base `300`, the demo exposes AV300 temperature (degrees Celsius), AV301
relative humidity (percent relative humidity), AV302 pressure (pascals), and
AV303 calculated dew point (degrees Celsius). A different base uses the same
four `+0` through `+3` offsets after restart.

Every AV advertises Object Identifier, Object Name, Object Type, Present Value,
Description, Units, Min/Max Present Value, Resolution, Status Flags, Event
State, Out Of Service, Reliability, and Property List. Min/Max are technical
measurement/display bounds, not warning or error thresholds.

The ConfigManager `Sensors` page contains a `BME280 BACnet Values` live card.
Its four labels include the configured AV instances and read the same retained
`Present_Value` storage as BACnet; temperature and dew point use `°C`, relative
humidity `%RH`, and pressure `Pa` without a separate sensor read or conversion.

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
