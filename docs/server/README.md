# Server Guide

The portable `BacnetServer` runtime accepts an injected
`BacnetDatagramTransport`, is driven by non-blocking `poll()`, decodes Who-Is,
emits I-Am, serves caller-owned objects through ReadProperty, and supports
WriteProperty for registered commandable Binary Outputs, Binary Values, Analog
Outputs, and Analog Values plus incoming SubscribeCOV and SubscribeCOVProperty
requests.

The active Device profile exposes its mandatory identity, protocol-capability,
and transport properties, an Object List, Property List, and an empty Device
Address Binding list. It returns BACnet errors for unknown objects/properties
and invalid array indices. Object and property lists support full, count
(`index 0`), and individual-entry reads.

## Device metadata opt-ins

The compact Device profile has no optional metadata storage. Applications may
independently add Device `Description`, `Location`, and `Serial_Number` through
caller-owned `BacnetServerPropertyRegistration` entries. Each entry must use
the Device object instance supplied to `begin()` and a CharacterString provider
whose context remains valid while the server runs. A registration is the sole
source for both ReadProperty and `Property_List`: an omitted metadata property
is neither advertised nor readable.

```cpp
const BacnetObjectId deviceObject{
  static_cast<uint16_t>(BacnetObjectType::Device), device.deviceInstance};
const BacnetServerPropertyRegistration metadata[] = {
  {deviceObject, BacnetPropertyId::Description, readText, description},
  {deviceObject, BacnetPropertyId::SerialNumber, readText, serialNumber},
};
server.setPropertyRegistrations(metadata, sizeof(metadata) / sizeof(metadata[0]));
server.begin(device);
```

There is no metadata bundle, default text, generated identifier, or hardware
lookup. `Device_UUID` is deliberately not exposed: the active Device profile
declares BACnet Protocol Revision 14, while Device_UUID was added for the
later BACnet/SC revision as a read-only 16-octet value. Supporting it would
require a protocol-profile expansion, not an optional text-property addition.

The resource comparison uses the same NodeMCU-32S WiFi server demo and
toolchain for the compact `usb` profile and `usb-device-metadata`, which
registers all three metadata properties:

| Profile | RAM | Flash | Delta from compact |
| --- | ---: | ---: | ---: |
| `usb` | 46,336 B | 765,141 B | baseline |
| `usb-device-metadata` | 46,336 B | 765,561 B | +0 B RAM, +420 B Flash |

The compact build does not compile the demo metadata strings or registration
array. The general server already borrows optional-property descriptors, so no
per-Device metadata fields or allocations are added to the base profile.

Analog Values are supplied in caller-owned `BacnetServerAnalogValue` storage
through `setAnalogValues()`. Registering no entries consumes no object-table
storage in the server and does not advertise Analog Value support. Each entry
provides Object Identifier, Object Name, Object Type, Present Value, Status
Flags, Event State, Out Of Service, Units, and Property List. The compact AV
profile reports only the implemented Out Of Service bit and a normal Event
State. Optional caller-owned `BacnetServerPropertyRegistration` entries add
only the properties they describe. They may override Status Flags or Event
State and add Reliability, Description, Min_Pres_Value, Max_Pres_Value, or
Resolution without reserving metadata storage in baseline AV entries.

The Present Value is read from the entry's stored `presentValue` when no
provider is configured. A configured `BacnetServerAnalogValueProvider` is a
function pointer plus caller context and is invoked only while serving a
Present Value read. The server neither owns the entries, strings, nor provider
context; all must remain valid while the server is running.

## Commandable analog objects

`BacnetServerAnalogValue` remains the compact read-only AV profile. Register
`BacnetServerCommandableAnalogValue` through
`setCommandableAnalogValues()` only for AV instances that need BACnet command
priority. The two AV forms can be registered together, but must not reuse an
instance number. Commandable AV storage owns its `BacnetCommandPriority<float>`
state and exposes `Present_Value`, `Priority_Array`, and
`Relinquish_Default`; ordinary AVs expose none of those commandable
properties.

`BacnetServerAnalogOutput` is a separate opt-in registration through
`setAnalogOutputs()`. AO and commandable AV registration are independent: an
application may enable either, both, or neither. Both types use priorities 1
through 16; omitted WriteProperty priority means 16, and a BACnet NULL
relinquishes only the selected slot. `Priority_Array` is read-only and reports
BACnet NULL for unoccupied slots. After every successful operation the highest
occupied priority determines `Present_Value`; when no slot is occupied,
`Relinquish_Default` is effective.

The AO `apply` callback is portable and optional. It receives only a changed,
committed effective value, never an individual priority slot. This keeps the
data path `Priority_Array -> effective priority -> Present_Value -> output
hook`; PWM, DAC, GPIO, and other hardware conversion remain platform-adapter
work. Commandable analog callback storage is also optional and follows the
existing synchronous ordering: output hook first, then priority-slot,
effective-priority, present-value, and relinquish callbacks where applicable.
Callbacks observe committed state, must not recursively mutate the same object,
and do not send COV traffic; the existing snapshot path is still the sole COV
change detector.

Property registrations are also caller-owned and use a function pointer plus
const context. Their presence is the single source for both `Property_List`
and ReadProperty: an unregistered property is neither advertised nor readable.
Optional runtime state can expose `inAlarm`/`fault` through Status Flags,
Low/High Limit Event State, and Reliability. It remains read-only and sends no
EventNotification; Out Of Service remains an AV field.

The runtime borrows the transport; the caller keeps that transport alive for
the server lifetime and does not share it between running server instances. A
caller may bind the portable monotonic clock for COV lifetimes and confirmed
notification retries. The normal request path does not allocate from the heap.

The ESP32 server demo uses the existing Arduino UDP adapter with separate WiFi
and Ethernet network setup, then injects it into this portable runtime. Its
shared profile is Device `1682127` plus AV200 (stored sine) and AV201
(polling/callback uptime). The demo-specific networking, identity, and value
binding remain outside the portable core.

Commandable Binary Outputs retain a caller-owned 16-slot priority array. A
Present Value write uses priority 16 when omitted; a BACnet NULL relinquishes
only the selected priority, and the highest active priority determines the
effective value or Relinquish Default. Local application writes are separate:
the server and object `setLocalWritePriority()` defaults apply only to
`BacnetBinaryOutput::writeValue()`, while an explicit local priority overrides
both. Local priority zero updates Relinquish_Default, and `relinquish()`
releases an explicit 1..16 slot. Priority Array itself is read-only.

Commandable Binary Values use the same caller-owned priority storage without
an output callback, polarity, reliability, or platform binding. Their compact
standard profile contains Object Identifier, Object Name, Object Type, Present
Value, Status Flags, Event State, Out Of Service, Priority Array, Relinquish
Default, and Property List. The paired
`server-esp-to-esp-demo-wifi` demo registers BV320 as its shared live-COV
instance.

## Portable bindings and callbacks

`BacnetAnalogInput::bindPresentValue()` remains the small binding for an
application-owned engineering value. `bindInput()` instead accepts a portable
function pointer and caller context for an actively read raw value. An optional
`BacnetLinearScale` maps that raw value to engineering units, clamps values
outside the raw range, and supports reversed ranges without adding a BACnet
property. Calling `bindPresentValue()` after configuring raw input switches
back to the direct engineering-value path and clears the previous raw-input
scale, so the bound variable is never interpreted as a raw input:

```cpp
BacnetLinearScale scale{0.0F, 4095.0F, 0.0F, 100.0F};
analogInput.bindInput(readRawValue, &sensorContext);
analogInput.setInputScale(scale);
analogInput.setUnits(BacnetEngineeringUnits::Percent);
```

Commandable Binary Outputs can use caller-owned callback storage when local
application code needs synchronous change notification. The storage and every
callback context must outlive the output. Outputs without callbacks retain only
one null storage pointer and allocate no listener list.

```cpp
BacnetBinaryOutputCallbackStorage callbacks;
output.attachCallbacks(callbacks);
output.onPresentValueChange(onPresentValue, &application);
output.onPriorityValueChange(8, onPrioritySlot, &application);
output.onRelinquish(onRelinquish, &application);
```

For a successful command, the object validates and commits the priority slot,
calculates the effective value, applies its existing output binding only when
that value changes, then invokes priority-slot, effective-priority,
present-value, and relinquish callbacks in that order when applicable. A
successful NULL release always invokes the relinquish callback, including a
non-effective slot. Output bindings and callbacks are synchronous, run from
normal server or local code rather than an ISR, must return quickly, and cannot
mutate the same output recursively. They do not send BACnet traffic or force
COV notifications; the existing polling snapshot remains the sole COV change
detector.

The server retains a fixed allocation-free COV subscription table. An
object-level subscription is one entry that encodes Present_Value and
Status_Flags together; a property subscription encodes only its requested
property. Confirmed COV notifications wait non-blockingly for SimpleACK.

See [Change of Value (COV)](../cov.md) for the supported Object-COV and
Property-COV forms, `COV_Increment`, lifetime/renewal/cancellation semantics,
bounded confirmed-notification retries, diagnostics, and scope boundaries.

This is not a complete BACnet/IP server feature: there is no EventNotification,
PWM/DAC/GPIO adapter, or generic real-I/O policy.

See [Planned Server Work](planned.md) for the current scope boundary.
