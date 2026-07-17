# Command Priority Reset Semantics

BACnet command priorities range from `1` (highest) through `16` (lowest).
Priority `6` is reserved for Minimum On/Off, priority `8` is commonly used for
manual operator commands, and priority `16` is the lowest standard command
priority.

To relinquish one command priority, write `Present_Value = Null` with that
explicit priority. A client must not write the `priority-array` property
directly. When no effective priority slot remains, the object uses its
`Relinquish_Default` value.

## Reset modes

`BacnetRemoteObject::relinquishAllPriorities()` is the strict reset mode. It
sends one explicit Null WriteProperty request for every priority from `1`
through `16` and stops at the first failure. Its result reports the successful
write count, the first failed priority, and the resulting WriteProperty status.
Use it to diagnose an object's complete externally writable priority range.

Passing `BacnetPriorityResetOptions` with
`skipMinimumOnOffPriority = true` selects the writable compatibility mode. It
sends exactly fifteen Null WriteProperty requests in this order:
`1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16`. The result explicitly
reports `skippedPriority = 6`; priority 6 is never skipped silently. This mode
is a practical reset of externally writable slots, not a claim that all sixteen
slots were written.

Both modes use the existing Present Value WriteProperty encoder and dispatch.
They send no datagrams unless both WriteProperty and priority writes are enabled
at compile time. Neither mode runs automatically or cyclically.

## Native Windows HIL

The internal `bacnet-write-hil` executable is a one-shot HIL utility, not a
productive CLI. It selects the requested Windows IPv4 interface, uses the
Winsock BACnet/IP transport, and runs the portable device-session priority
helpers against explicit command-line targets. It does not store local network
addresses or test-object instances in repository configuration. The utility
uses priority 8 for its temporary command, relinquishes an active temporary
priority after a failed readback, and performs the writable reset only for its
requested Analog Value target. After a successful temporary priority write, a
failed primary relinquish triggers exactly one cleanup relinquish. The primary
and cleanup statuses are reported separately; a failed cleanup reports that an
override may remain active and stops the run before another target is written.

## Device compatibility note

Devices can apply different or stricter access policies to command priorities.
Priority 6 is intended for Minimum On/Off and can be rejected for external
writes; this is not a universal rule for Analog Value or Multi-State Value
objects.

[NOTE] Local WAGO HIL observation, not normative behavior: Device Instance
`1682101`, object `AV200`, rejected `Present_Value = Null` at priority `6`
with BACnet error `write-access-denied`, mapped by this client to
`NotCommandable`. The behavior remained after the WAGO cyclic priority-16
writer was stopped.
