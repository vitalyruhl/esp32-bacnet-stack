# BACnet I/O Example

This ESP32 WiFi example binds the development station's physical inputs to the
portable BACnet server without introducing Arduino dependencies into the core.
It is read-only for sensors and buttons, and provides exactly two commandable
Binary Outputs for the development-station LEDs. Relays, PWM, network-loss
failsafes, limit alarms, and Intrinsic Reporting remain outside this slice.

## BACnet profile

| Object | Instance | Name | Source | Units |
| --- | ---: | --- | --- | --- |
| Analog Input | 0 | Light Sensor | LDR voltage divider | Percent |
| Analog Input | 1 | Temperature | DS18B20 | Degrees Celsius |
| Binary Input | 0 | Reset Button | GPIO14, low-active | — |
| Binary Input | 1 | Mid Button | GPIO33, low-active | — |
| Binary Input | 2 | Set Button | GPIO4, low-active | — |
| Binary Output | 0 | LED 1 | GPIO25, active-high by default | — |
| Binary Output | 1 | LED 2 | GPIO26, active-high by default | — |

The two AIs expose Object Identifier, Object Name, Object Type, Present Value,
Status Flags, Event State, Out Of Service, Reliability, Units, Description,
Min/Max Present Value, Resolution, and Property List. The BIs expose the same
read-only object/state properties except Units and engineering bounds.
Each BO exposes Object Identifier, Object Name, Object Type, Present Value,
Description, Status Flags, Event State, Out Of Service, Reliability, Polarity,
Priority Array, Relinquish Default, and Property List. Present Value writes
accept BACnet `ACTIVE`/`INACTIVE` or `NULL`; `NULL` relinquishes precisely the
selected priority. An omitted priority means 16. Priority Array itself is
read-only.

The LDR uses ADC1 GPIO36 and scales clamped ADC calibration endpoints to the
configured technical range (default `0.0..100.0 %`, resolution `0.1`). Equal
calibration values, non-finite technical limits, invalid GPIOs, duplicate pins,
and reserved-pin conflicts mark only the affected input faulty or out of
service; they do not stop the BACnet server. DS18B20 absence, invalid readings,
or a disconnected bus expose `Reliability=no-sensor`, fault Status_Flags, and
fault Event_State. A failed DS18B20 never replaces the retained last valid
temperature with a fabricated value.

All buttons use `INPUT_PULLUP`; pressed is BACnet `ACTIVE`. Debouncing uses
`millis()` and never calls `delay()`.

## Configuration

ConfigManager persists the BACnet Device/port, LDR enable/GPIO/raw endpoints,
LDR inversion/technical range, DS18B20 enable/GPIO, and for each button:
enable/GPIO/inversion/debounce time. Each LED also has enable/GPIO,
active-low inversion, and Relinquish Default settings. GPIO and binding changes
are restart settings. Invalid or duplicate LED bindings are Out Of Service and
never drive a GPIO. An Out Of Service BO preserves its logical priority state,
but its physical binding is held inactive. The OLED remains physically connected
on I2C (`SDA=GPIO21`, `SCL=GPIO22`, address `0x3C`) but is deliberately not
initialized by this firmware. The BME280 is also physically present on this I2C
bus but is not a measurement source in this example.

## Wiring and safety

The authoritative Wokwi development-station diagram and connection reference
remain in [`dev-info/Wokwi`](../../dev-info/Wokwi) and
[`dev-info/IO-example-esp32-connection.md`](../../dev-info/IO-example-esp32-connection.md).
It includes the two commandable LEDs and the reserved, currently un-driven
relays; Relay 1 is on GPIO19. GPIO12 remains an unused strapping pin. GPIO4 is the confirmed `SET`
button input and remains a strapping pin. Avoid forcing it to an unsafe level
during reset or boot.

## Build and HIL

```sh
pio run -d examples/io-example -e wifi-com4
pio run -d examples/io-example -e wifi-com4 -t upload
pio device monitor -p COM4 -b 115200
```

Read-only input HIL was accepted: the WAGO BACnet Configurator recognized the ESP32,
AI0/AI1 returned plausible LDR/DS18B20 values and all three BIs operated.
LED-output HIL, including safe startup behavior, remains pending for this slice.
