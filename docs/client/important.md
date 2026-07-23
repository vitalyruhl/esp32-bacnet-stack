# Important Client Notes

## Runtime Model

- The library does not spawn background tasks for client polling.
- Applications must drive BACnet progress from `loop()`.
- `BacnetPropertySubscription` callbacks run synchronously inside `BacnetDeviceSession::poll()`.

## Ownership And Lifetime

- `BacnetClient` uses a caller-owned `BacnetDatagramTransport` and owns its
  logger. Arduino projects bind `ArduinoUdpDatagramTransport` and may use
  `ArduinoMonotonicClock` from `ArduinoBacnetClient.h`. The compatibility
  constructor requires `setTransport()` before `begin()`.
- `BacnetDeviceSession` stores target identity and uses a caller-owned `BacnetClient`.
- `BacnetPropertySubscription` must not outlive the `BacnetDeviceSession` that created it.
- Callback notification pointers are valid only during the callback; copy retained values explicitly.

## Discovery And Sessions

- Discovery is based on Who-Is / I-Am over BACnet/IP.
- Known targets can use the public `BacnetDeviceSession` constructor or the factory helpers `fromEndpoint()` and `fromIAm()`.
- The library does not maintain a global discovered-device registry.

## Read And Scan Behavior

- `readProperty()` is a blocking convenience API for simple known-property reads.
- `scanObjectList()` is the blocking convenience path for Device `object-list` scanning.
- `beginObjectListScan()` and `pollObjectListScan()` provide the non-blocking scan path.
- One session processes one object-list scan job at a time.
- Object-list scan options are stored by value, but pointed-to filter arrays remain caller-owned and must stay alive for the job duration.

## Subscription And Fallback Behavior

- `BacnetProperty::subscribe()` provides a property subscription abstraction.
- Set `BacnetSubscribeOptions::preferCov` to request COV, and
  `usePropertyCov` for SubscribeCOVProperty. See [Change of Value (COV)](../cov.md)
  for confirmation, lifetime, cancellation, retry, and fallback semantics.
- `fallbackPollMs = 0` disables periodic fallback polling.
- `requestRefresh()` schedules an immediate one-shot refresh when the subscription is idle.
- One session processes at most one subscription read in flight.

## Safety And Boundaries

- WriteProperty is an explicit compile-time opt-in client API and is disabled
  by default. Priority writes require the separate priority-write opt-in.
  Hardware write validation remains a local, explicit HIL action.
- Command priority reset helpers do not run automatically; applications invoke
  them explicitly. See [Command priority reset semantics](../bacnet-command-priority.md).
- The library must remain vendor-neutral and must not hardcode WAGO-specific assumptions in reusable APIs.
- ConfigManager is optional demo infrastructure and is not a library dependency.
- The rich client demo keeps object/property presentation bounded. After an
  explicit browser action it reads up to eight advertised properties; a small
  known-property profile is only a compatibility path for a correctly addressed
  object that explicitly reports `Property_List` unsupported. It never writes
  during startup, discovery, scan, refresh, polling, COV handling, reconnect,
  resubscription, or settings persistence.

## Transport Scope

- BACnet/IP is the current implementation target.
- BACnet MS/TP is planned later and must not be assumed implemented in this repository.
