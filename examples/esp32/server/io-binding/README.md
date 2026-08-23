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
| Analog Output | 0 | PWM Output | GPIO32 PWM duty | Percent |
| Analog Value | 0 | Analog Setpoint | BACnet-only setpoint | Percent |

AI and BI Present_Value is read-only. BO Present_Value remains commandable.
Incoming BACnet WriteProperty requests use their requested priority exactly,
or priority 16 when omitted; `NULL` relinquishes that same slot. Local ESP
application writes are separate: `BacnetServer::setLocalWritePriority()` sets
their server fallback, `BacnetBinaryOutput::setLocalWritePriority()` overrides
it for one output, and `writeValue(value, priority)` overrides both for one
call. Local priority `0` updates Relinquish_Default, while `relinquish(1..16)`
explicitly releases a slot. Present_Value and the IOManager output always use
the effective Priority_Array result; a lower, hidden slot never produces a
false Present_Value change. Priority 1 is not an access lock: a permitted
BACnet client can still write or relinquish that same slot. Priority_Array is
read-only.

AO0 and AV0 are separate opt-in commandable analog objects. Both use the same
BACnet priority and NULL-relinquish semantics as BO. AO0 is the physical path:
only its committed effective value is converted to an 8-bit PWM duty on GPIO32.
AV0 is a logical setpoint and has no hardware binding. AO0 starts and returns
to its 0% Relinquish_Default after the last active priority is relinquished.
GPIO25 and GPIO26 remain exclusively owned by the accepted BO LEDs, so this
WiFi station deliberately does not register a conflicting ESP32 DAC output.

## IOManager bindings and Live I/O

The example uses ConfigManager 4.4.10 `cm::IOManager` from `src/io/IOManager.h`.
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
service, object, property, age, bounded object read/write counters, and the
first active COV subscription (client endpoint, process ID, object/property,
notification mode, remaining lifetime, state, last send, and last
acknowledgement).
Those diagnostics are RAM-only and are not BACnet properties or persistent
settings.

The AO0 PWM binding remains example-local. It validates that GPIO32 is a safe,
unowned ESP32 PWM-capable output before configuring it, initializes it to a
safe 0% duty, and receives only the post-priority effective value through the
portable AO callback. It never runs from an ISR. The existing GPIO25/GPIO26
LED and ConfigManager bindings are unchanged.

BACnet/IP is UDP-based: the UI reports `Recent BACnet activity` or `No recent
BACnet activity`, never a false connected/disconnected client state. The server
accepts a fixed, allocation-free table of eight incoming SubscribeCOV and
SubscribeCOVProperty subscriptions. Object-level subscriptions are one table
entry and always notify Present_Value and Status_Flags together; property
subscriptions remain restricted to their requested property. Subscriptions
notify initially and on change, expire or cancel through their requested
lifetime, and support confirmed notifications with a non-blocking
acknowledgement wait. A real-valued Present_Value property subscription can
also supply COV_Increment; sub-threshold changes do not notify.

The current WBC observation is intentionally not treated as COV acceptance:
its active COV subscription count is 0, its latest service is ReadProperty,
and it polls at roughly two seconds. That client-side polling delay does not
measure the server COV path.

The example keeps Vendor ID `0` as an explicit development configuration. Set
`BACnet Vendor ID (restart required)` to the legitimately assigned Vendor ID of
the responsible product provider before interoperating as a product. The same
configured value is used by both I-Am and Device Vendor_Identifier.

## Build and HIL

```sh
pio run -d examples/esp32/server/io-binding -e wifi-com4
pio run -d examples/esp32/server/io-binding -e wifi-com4 -t upload
pio device monitor -p COM4 -b 115200
```

The requested user regression is intentionally short: open ConfigManager Live
I/O, verify light and temperature, press one button, switch one LED through
BACnet Present_Value, and read an object from the WAGO client to confirm last
peer/request activity. For AO0 HIL, attach only a high-impedance scope or
meter, or an RC-filtered indicator, to GPIO32; do not use the reserved relay
inputs as a PWM load. Verify priority 16, priority 8 override, relinquish 8,
relinquish 16, and the 0% Relinquish_Default. Repeat the priority/relinquish
readback for AV0 without a hardware observation.

### Commandable analog HIL result

The local HIL on 2026-08-09 passed. A high-impedance meter measured AO0
directly between GPIO32 and GND: 0% was approximately -0.12 mV, 50% was
1.63 V, and 100% was 3.26 V. Those values confirm the expected PWM average
voltage and a plausible linear transfer across the 3.3 V output range.

The Ethernet client also confirmed the logical path: priority 16 establishes
the effective AO0/AV0 value, priority 8 overrides it, relinquishing priority 8
restores priority 16, and relinquishing priority 16 restores the 0%
Relinquish_Default. The Live UI and COV diagnostics remained responsive, with
no observed callback or reentrancy loop.
