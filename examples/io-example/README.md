# BACnet I/O Example

This ESP32 WiFi example is a readable reference for binding application values
to portable BACnet server objects. Native GPIO and ADC work remains in
ConfigManager IOManager; the reusable BACnet core has no ConfigManager, GPIO,
Arduino, or ESP32 dependency.

## Object-centric BACnet configuration

Each object has a small named registration function in `src/main.cpp`. The
normal path binds an application value and adds only optional properties:

```cpp
lightSensor.configure(0, "Light Sensor");
lightSensor.bindPresentValue(&lightValue);
lightSensor.setUnits(BacnetEngineeringUnits::Percent);
lightSensor.addProperty(BacnetPropertyId::Description, "LDR light level");
lightSensor.addProperty(BacnetPropertyId::MinPresentValue, 0.0F);
lightSensor.addProperty(BacnetPropertyId::MaxPresentValue, 100.0F);
lightSensor.addProperty(BacnetPropertyId::Resolution, 0.1F);
bacnetServer.addObject(lightSensor);
```

`BacnetAnalogInput`, `BacnetBinaryInput`, and `BacnetBinaryOutput` are
allocation-free facades over the existing caller-owned `BacnetServer*`
structures. The established low-level array API remains compatible for
advanced, generated, and loop-based definitions. `addProperty()` returns an
explicit status when a type is wrong, a property is unsupported, a property is
duplicated, or capacity is exhausted. Normal example code does not need to
check every return value: the object retains its first configuration error and
`addObject()` rejects that object without registering it. The object error
contains its name, Object Identifier, property, and reason, so the example
logs it once at registration without duplicate user-authored error text.

The server already provides each object's identity, type, Present_Value,
Status_Flags, Event_State, Out_Of_Service, Property_List, analog Units, and
the commandable BO Priority_Array and Relinquish_Default. Optional properties
in this example add descriptions, engineering bounds, resolution, and input
health metadata.

`Min_Pres_Value` and `Max_Pres_Value` are lower and upper technical operating
limits. `Resolution` is the smallest meaningful displayed change. They are not
warning or error limits; future alarming must use the distinct BACnet limit,
deadband, reliability, and intrinsic-reporting features.

## BACnet profile

| Object | Instance | Name | Bound application value | Units |
| --- | ---: | --- | --- | --- |
| Analog Input | 0 | Light Sensor | IOManager scaled LDR value | Percent |
| Analog Input | 1 | Temperature | DS18B20 temperature | Degrees Celsius |
| Binary Input | 0 | Reset Button | IOManager digital input | — |
| Binary Input | 1 | Mid Button | IOManager digital input | — |
| Binary Input | 2 | Set Button | IOManager digital input | — |
| Binary Output | 0 | LED 1 | IOManager digital output | — |
| Binary Output | 1 | LED 2 | IOManager digital output | — |

AI and BI Present_Value is read-only. BO Present_Value remains commandable:
writes use BACnet priorities 1 through 16, `NULL` relinquishes an individual
slot, and the IOManager output follows the effective value. Priority_Array is
read-only.

## IOManager bindings and Live I/O

The example uses ConfigManager 4.4.1 `cm::IOManager` from `src/io/IOManager.h`.
The LDR binding is registered as:

```cpp
ioManager.addAnalogInput("ldr_s", "LDR light level", 36, true,
                         0, 4095, 0.0F, 100.0F, "%", 1);
```

The parameters are ID, UI name, ADC pin, persistent-settings flag, raw minimum,
raw maximum, engineering minimum, engineering maximum, unit, and display
precision. IOManager updates the ADC and engineering values non-blockingly;
`getAnalogRawValue("ldr_s")` and `getAnalogValue("ldr_s")` provide the two
representations. Its Live UI displays the LDR's scaled and raw values and the
three digital button states. No second raw range, GPIO validity table, or ADC
scaling formula exists in this example.

IOManager also owns the native GPIO/polarity settings for the buttons and LEDs.
The DS18B20 is an external sensor rather than an IOManager-native channel, so
it keeps a small example-only ConfigManager binding.

Current IOManager 4.4.1 does not provide persisted enable settings for digital
outputs or cross-channel duplicate-pin detection after arbitrary web UI edits.
Those are documented follow-up gaps; this example does not recreate a parallel
I/O manager or pin database.

The ConfigManager Live UI adds a separate BACnet card with commandable LED
state and effective priority, and a BACnet activity card with last peer,
service, object, property, age, and bounded object read/write counters. Those
diagnostics are RAM-only and are not BACnet properties or persistent settings.

BACnet/IP is UDP-based: the UI reports `Recent BACnet activity` or `No recent
BACnet activity`, never a false connected/disconnected client state. The server
does not currently implement incoming SubscribeCOV subscriptions, so the UI
explicitly reports `Server-side COV not supported`.

## Build and HIL

```sh
pio run -d examples/io-example -e wifi-com4
pio run -d examples/io-example -e wifi-com4 -t upload
pio device monitor -p COM4 -b 115200
```

The requested user regression is intentionally short: open ConfigManager Live
I/O, verify light and temperature, press one button, switch one LED through
BACnet Present_Value, and read an object from the WAGO client to confirm last
peer/request activity. Do not claim a complete HIL result until those physical
observations have been made.
