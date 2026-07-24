# Server Guide

The portable `BacnetServer` runtime accepts an injected
`BacnetDatagramTransport`, is driven by non-blocking `poll()`, decodes Who-Is,
emits I-Am, serves caller-owned objects through ReadProperty, and supports
WriteProperty for registered commandable Binary Outputs and Binary Values plus incoming
SubscribeCOV and SubscribeCOVProperty requests.

The active Device profile exposes its mandatory identity, protocol-capability,
and transport properties, an Object List, Property List, and an empty Device
Address Binding list. It returns BACnet errors for unknown objects/properties
and invalid array indices. Object and property lists support full, count
(`index 0`), and individual-entry reads.

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
`BacnetLinearScale` maps that value to engineering units, clamps values outside
the raw range, and supports reversed ranges without adding a BACnet property:

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
non-effective slot. Callbacks are synchronous, run from normal server or local
code rather than an ISR, must return quickly, and cannot mutate the same output
recursively. They do not send BACnet traffic or force COV notifications; the
existing polling snapshot remains the sole COV change detector.

The server retains a fixed allocation-free COV subscription table. An
object-level subscription is one entry that encodes Present_Value and
Status_Flags together; a property subscription encodes only its requested
property. Confirmed COV notifications wait non-blockingly for SimpleACK.

See [Change of Value (COV)](../cov.md) for the supported Object-COV and
Property-COV forms, `COV_Increment`, lifetime/renewal/cancellation semantics,
bounded confirmed-notification retries, diagnostics, and scope boundaries.

This is not a complete BACnet/IP server feature: there is no EventNotification,
Analog Output/PWM, or generic real-I/O policy.

See [Planned Server Work](planned.md) for the current scope boundary.
