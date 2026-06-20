# Reusable BACnet Client And Server Layer Plan

## Scope

This plan describes the next BACnet/IP library layers after the minimal
ReadProperty and generic property-access work. The goal is to move protocol,
scan, transaction, polling, timeout, decode, cache, and logging dispatch state
out of applications and examples.

Normal application code should configure the library, create or discover device
sessions, read properties, subscribe or poll values, and receive callbacks. It
should not manually own invoke IDs, request state, retry timing, scan cursors,
ACK parsing, or BACnet log routing.

The library remains vendor-neutral. WAGO addresses, device IDs, and AV/MV scan
ranges are example defaults and validation notes only.

## Current State Summary

- `BacnetClient` already provides generic object/property request helpers,
  typed `BacnetValue` decoding, Who-Is/I-Am, and ReadProperty send/poll helpers.
- `examples/client-demo` still owns device-specific scan configuration, active
  ReadProperty state, invoke ID allocation, timeout handling, scan progression,
  value refresh, GUI formatting, and ConfigManager log forwarding.
- Tests cover low-level request encoding/decoding and selected typed value
  parsing, but not reusable session, scan, subscription, or log dispatch layers.

## Design Principles

- Keep transport, BACnet encoding/decoding, transaction state, client sessions,
  scans, subscriptions, and logging as separate layers with narrow interfaces.
- Keep application-facing APIs non-blocking and loop-driven for Arduino/ESP32.
- Use explicit status objects for pending, success, timeout, protocol error,
  decode error, unsupported property, and cancellation.
- Treat ConfigManager, Serial, MQTT, file output, and GUI output as application
  adapters. The BACnet library must not depend on ConfigManager.
- Prefer bounded state and compile-time capacities suitable for embedded use.
- Add WriteProperty, MS/TP, full object browsing, and full COV only in later
  scoped steps.

## Proposed Layers And Responsibilities

### 1. BACnet Transport / UDP Boundary

`BacnetUdpTransport` should own the UDP socket and endpoint metadata.

Responsibilities:

- Begin/end UDP on a configured local port.
- Send datagrams to a unicast or broadcast BACnet/IP endpoint.
- Receive datagrams and expose remote IP, remote port, local timestamp, and
  payload bytes.
- Hide `WiFiUDP` from higher layers except through a small transport interface.
- Allow future test transports or alternate IP transports without touching the
  transaction manager.

The transport does not parse APDU services or manage invoke IDs.

### 2. BACnet Encode/Decode Boundary

`BacnetCodec` should contain BVLC, NPDU, APDU, object identifier, property
identifier, array index, and application-tag encoding/decoding.

Responsibilities:

- Encode Who-Is, I-Am, ReadProperty requests, ReadProperty ACKs, and protocol
  error responses as needed.
- Decode incoming APDUs into small typed frame/result structures.
- Convert application-tag payloads into `BacnetValue`.
- Preserve enough metadata for diagnostics: PDU type, service choice, invoke ID,
  object ID, property ID, array index, and rejection reason.

The codec must remain stateless except for optional scratch buffers supplied by
the caller. It should not own network sockets, timers, sessions, or callbacks.

### 3. Transaction / Request Manager

`BacnetTransactionManager` should own outstanding confirmed requests.

Responsibilities:

- Allocate and wrap invoke IDs safely.
- Track request target endpoint, service, encoded request metadata, start time,
  timeout, retry policy if later added, and completion callback.
- Match ACK, Error, Reject, or Abort responses to the correct transaction by
  invoke ID and source endpoint.
- Report unexpected APDUs, unexpected invoke IDs, malformed responses, decode
  failures, and timeouts.
- Limit concurrency through a compile-time or runtime capacity.

Applications should not allocate invoke IDs or poll individual ReadProperty
responses. They should receive request completion through handles, callbacks, or
cached property state.

### 4. Reusable Client API

`BacnetClient` should stay the small application-facing owner of core client
runtime state. Object enumeration and browser-style state should be optional
layers, not mandatory `BacnetClient` behavior.

Responsibilities:

- Own transport, codec usage, transaction manager, discovered devices, and
  BACnet logger attachment.
- Provide `begin()`, `end()`, `poll()`, `isRunning()`, `sendWhoIs()`, discovery
  callbacks, and low-level ReadProperty operations.
- Dispatch incoming datagrams to discovery or transaction paths.
- Tick transaction timeouts and log outputs from `poll()`.
- Keep low-level encode/decode helpers available where useful for tests, but
  keep normal application code on higher-level APIs.

The core client should not automatically enumerate all objects, own GUI scan
state, or require object caches for users that only read or write known BACnet
points.

### 5. Device Session API

`BacnetDeviceSession` should represent a known remote BACnet device.

Identity and ownership:

- Device instance ID.
- BACnet/IP endpoint: IP address and UDP port.
- Vendor ID when known.
- Last I-Am timestamp and discovery status.
- Cached device properties and read statuses.
- Owned by `BacnetClient`; application code may hold lightweight handles or
  pointers with documented lifetime.

Responsibilities:

- `readProperty(BacnetPropertyRequest, callback/options)`.
- `object(BacnetObjectId)` to create object handles.
- `scan(BacnetScanOptions)` to start a scan job.
- `subscribe(BacnetPropertyRequest, BacnetSubscribeOptions, callback)`.
- Coordinate per-device request limits and logging context.

Sessions must support both discovered devices and manually configured devices.
Manual configuration is useful for validation, but must not hardcode WAGO
assumptions in the library.

### 6. Object / Property API

`BacnetRemoteObject` and `BacnetProperty` should provide ergonomic handles over
the generic request model.

Core value types:

- `BacnetObjectId`: object type and instance.
- `BacnetPropertyId`: named enum for common properties such as `object-name`,
  `description`, `present-value`, `property-list`, `object-list`,
  `number-of-states`, `state-text`, `priority-array`, and
  `relinquish-default`.
- `BacnetPropertyRequest`: object ID, property ID, and optional array index.
- `BacnetValue`: typed value plus display conversion.
- `BacnetPropertyStatus`: last request state and error detail.

Object handle helpers:

- `property(propertyId, optionalArrayIndex)`.
- `readObjectName()`.
- `readDescription()`.
- `readPresentValue()`.
- `readPropertyList()`.

Property handle helpers:

- `read(callback/options)`.
- `poll(intervalMs)`.
- `subscribe(options, callback)`.
- `lastValue()`.
- `lastStatus()`.

Unsupported or missing properties should fail only the specific property read,
not the whole object or scan.

### 7. Optional Object Discovery / Scan API

Object discovery should live in an optional bounded helper layer such as
`BacnetObjectDiscovery`, `BacnetObjectEnumerator`, or a future session-owned
scan job. It must not become mandatory `BacnetClient` core behavior.

Configuration:

- Device session.
- Primary discovery source: Device Object `object-list` when available.
- Optional debug/fallback instance ranges per object type.
- Maximum result count per type and total if needed.
- Maximum `object-list` entries to inspect.
- Probe delay/yield interval.
- Read timeout.
- Optional properties to read for each found object.
- Optional callback for object found, progress, and completion.

State representation:

- Scan status: idle, running, complete, canceled, failed.
- Current object-list index or fallback object type/instance, current property,
  and pending transaction handle.
- Found objects as `BacnetScannedObject` records with object ID, selected label,
  property values, and per-property statuses.
- Counters for probed instances, found objects, timeouts, unsupported responses,
  and decode failures.

The scan should be non-blocking and advanced by the optional helper or the
application demo loop. The core client only supplies ReadProperty and logging
operations.

For the WAGO validation demo, AV200 and MV2000 are examples of existing object
IDs, not the normal discovery strategy. Range probing may remain disabled
fallback/debug logic only.

### 8. Subscription / Fallback Polling API

`BacnetSubscription` should exist before full BACnet COV support.

Representation:

- Target session and property request.
- Callback function and optional user context.
- Last value, last status, last update time, and next poll time.
- Options including fallback polling interval and future COV preference fields.
- Active/canceled state.

Fallback rules:

- Default fallback polling interval is 10000 ms.
- `fallbackPollMs > 0` uses the requested interval.
- `fallbackPollMs == 0` explicitly disables fallback polling.

The first implementation may use fallback polling only. Later COV support can
reuse the same subscription handle and callback path while adding confirmed or
unconfirmed COV mechanics internally.

### 9. Logging / Event Output API

`BacnetLogger` should follow the same output model as the ConfigManager logging
reference while remaining independent of ConfigManager.

Compile-time flags:

- `BACNET_ENABLE_LOGGING`
- `BACNET_ENABLE_VERBOSE_LOGGING`

Runtime concepts:

- Global log level.
- Zero, one, or multiple outputs.
- Per-output level.
- Per-output filter.
- Per-output timestamp mode.
- Per-output rate limit.
- Explicit tags, for example `BACnet`, `BACnet/Discovery`,
  `BACnet/ReadProperty`, `BACnet/Scan`, `BACnet/Subscription`,
  `BACnet/Server`.
- Scoped tags for nested operations.

Output interface:

- A small `BacnetLogOutput` base class can receive `BacnetLogRecord` values.
- The record should include level, tag, message, timestamp, and optional event
  metadata where memory allows.
- Outputs expose a `tick()` method so queued GUI or network outputs can flush
  incrementally from `BacnetClient::poll()` or application loop code.

Application adapters:

- Serial output adapter.
- ConfigManager GUI output adapter in `examples/client-demo`.
- MQTT output adapter in application code.
- File output adapter in application code.

The library should log important events but must not log credentials or local
configuration secrets.

Required client log events:

- Client start/stop and UDP port.
- Who-Is destination and send result.
- I-Am device ID, source endpoint, and vendor ID.
- ReadProperty queued/sent, ACK value, error, reject, abort, decode failure, and
  timeout.
- Scan start, ranges, found objects, zero-object warnings, and completion
  counts.
- Subscription creation, fallback poll reads, value updates, failures, and
  cancellation.

### 10. Future Server API

The server should use the same design principle: application code describes the
model, while the library owns protocol mechanics.

Proposed classes:

- `BacnetServer`: owns transport, codec use, logger, and request dispatch.
- `BacnetDeviceModel`: device identity and device-level properties.
- `BacnetObjectModel`: object type, instance, and property registry.
- `BacnetPropertyModel`: static value, value provider callback, optional write
  handler, flags, and priority handling hooks for later work.
- `BacnetServerRequestContext`: remote endpoint, invoke ID, service, and logging
  context.

Responsibilities:

- Parse incoming BACnet requests.
- Locate objects/properties in the registered model.
- Call value providers or write handlers.
- Encode ACKs and protocol errors.
- Log server receive, lookup, encode, error, and timeout events.
- Reuse `BacnetValue`, object IDs, property IDs, codec utilities, and logging.

Normal server applications should not manually encode BACnet frames.

### 11. Thin Examples And Demos

`examples/client-demo` should keep only application-specific glue:

- WiFi and ConfigManager setup.
- Example WAGO validation defaults such as IP address, device ID, and AV/MV
  scan ranges.
- GUI layout and display binding.
- ConfigManager GUI log output adapter.
- Serial output setup if desired.
- Calls into `BacnetClient`, `BacnetDeviceSession`, scan, read, and subscription
  APIs.

It should not own:

- Invoke ID allocation.
- Active transaction state.
- ReadProperty ACK matching.
- Timeout handling.
- Scan cursors.
- Fallback polling state.
- BACnet value decoding.
- Library log fan-out.

## Ownership And Lifetime Model

- The application owns a `BacnetClient` instance.
- `BacnetClient` owns its transport, transaction manager, logger, session store,
  scan jobs, and subscriptions.
- `BacnetDeviceSession` objects are owned by `BacnetClient`.
- `BacnetRemoteObject`, `BacnetProperty`, and subscription handles are
  lightweight references to client-owned state.
- Cancellation must be explicit and cheap. Destroying a handle should not leave
  a dangling callback.
- Callback user data should be opaque and application-owned.
- Default capacities should be small and documented, with compile-time override
  points for sessions, transactions, subscriptions, scan results, and log
  outputs.

## Non-Blocking Loop Model

Applications call `client.poll()` frequently from the main loop.

`poll()` should:

- Drain a bounded number of UDP packets.
- Dispatch discovery frames and confirmed-service responses.
- Advance transaction timeouts.
- Advance active scan jobs by at most a bounded amount.
- Trigger due fallback-poll subscriptions.
- Dispatch ready callbacks.
- Tick log outputs.

Long scans must be throttled through scan options and internal scheduling so the
ESP32 web UI and other application logic remain responsive.

## Callback Dispatch

Callbacks should be dispatched from `BacnetClient::poll()`, not from interrupts
or transport receive internals.

Callback types:

- Discovery callback: device found or updated.
- Read callback: one property read completed.
- Scan callback: object found, progress, complete, or canceled.
- Subscription callback: value update or status change.
- Logging callback/output: log record emitted.

Callbacks should receive enough context to avoid global lookups: session handle
or device ID, object ID, property ID, optional array index, value/status, and
timestamp.

The API should document whether callbacks may start new reads immediately. The
safer default is to allow callbacks to schedule work through the client while
the transaction manager defers actual send work until the current dispatch pass
finishes.

## What Moves Into The Library

- Remote endpoint-aware UDP transport abstraction.
- ReadProperty transaction ownership and invoke ID matching.
- Request timeout handling.
- Typed value decode status.
- Device session registry.
- Object/property handles and cached last value/status.
- Non-blocking scan state machine.
- Subscription and fallback polling state.
- BACnet logging fan-out and filters.
- Protocol event logging.

## What Remains In `examples/client-demo`

- WiFi and ConfigManager initialization.
- Example hardware defaults and validation ranges.
- Presentation of first discovered device and first found AV/MV rows.
- Application-side conversion from library records to GUI rows.
- ConfigManager log output adapter.
- Human-readable validation notes.

## Implementation Sequence

### Phase 0: Planning

- Add this architecture plan.
- Do not change product code.
- Do not bump version.

### #21: Logging Layer

- Add `BacnetLogLevel`, `BacnetLogRecord`, `BacnetLogOutput`, and a small
  `BacnetLogger`.
- Add compile-time flags `BACNET_ENABLE_LOGGING` and
  `BACNET_ENABLE_VERBOSE_LOGGING`.
- Support global level, multiple outputs, per-output level/filter/timestamp/rate
  limit, explicit tags, and scoped tags.
- Keep the library usable with no outputs attached.
- Refactor current BACnet Serial-only/demo logging into application adapters.

### #22: Device Session API

- Add session ownership in `BacnetClient`.
- Add discovered and manually configured device sessions.
- Move per-device metadata and cached device properties into session state.
- Introduce transaction manager ownership for ReadProperty state and invoke IDs.

### #23: Object / Property API

- Promote the generic request/value model into ergonomic object and property
  handles.
- Add property read statuses and cached last values.
- Support optional array indexes through the public API.
- Keep unsupported property reads local to the requested property.

### #24: Scan API

- Add non-blocking scan options, scan job state, scan result objects, and scan
  callbacks.
- Move AV/MV range scanning out of `examples/client-demo`.
- Keep WAGO validation ranges in the demo only.
- Log found objects and zero-result warnings.

### #25: Subscription / Fallback Polling API

- Add `BacnetSubscribeOptions` and subscription handles.
- Implement fallback polling first with the required default of 10000 ms.
- Treat `fallbackPollMs == 0` as an explicit fallback-disable setting.
- Dispatch value/status changes through callbacks from `poll()`.

### #26: Demo Refactor

- Rewrite `examples/client-demo` as a thin consumer of session, scan,
  property, subscription, and logging APIs.
- Keep ConfigManager integration as an application adapter only.
- Display the first discovered device and first found AV/MV objects without
  demo-owned BACnet protocol state.

### #3 And #6: Server MVP And Server Example

- After shared codec, value, logging, and transport boundaries are stable, add
  the server model API.
- Implement server request parsing, object/property lookup, response encoding,
  and protocol error generation inside the library.
- Add the basic server example after the server API can serve static/provider
  values without application-side frame encoding.

### #7: Client Example

- Once #22 through #25 are available, add or simplify a basic client example
  that demonstrates discovery, one property read, and one fallback-polled
  subscription without ConfigManager.

## Risks And Open Questions

- Capacity policy: fixed arrays, dynamic allocation, or configurable hybrid
  limits must be decided for sessions, transactions, subscriptions, scan jobs,
  and log outputs.
- Transport metadata: response matching needs reliable remote endpoint capture
  for every received packet.
- Callback lifetime: subscription cancellation and object/property handles must
  not leave dangling callbacks.
- Callback reentrancy: the library should define whether callbacks may request
  more work immediately or only schedule work for the next poll pass.
- BACnet value coverage: current typed decode support is intentionally narrow;
  unsupported tags must remain visible as clean statuses.
- Segmentation: large property values or object lists may require future
  segmented-response support, but scans should not depend on it initially.
- Transaction concurrency: initial implementation may allow one or a small
  bounded number of confirmed requests per device.
- Logging memory cost: output queues, startup buffers, and rate limits need
  embedded-friendly defaults.
- Server write handling: future server property write semantics and priority
  array behavior need a separate design before WriteProperty is implemented.
- COV migration: fallback polling subscriptions should not block a later COV
  implementation from using the same public handles.

## Issue Update Assessment

Issues #20 through #26 already describe the main work slices well. Important
details from this plan that may be worth adding later are:

- #21: per-output timestamp mode, filter, rate limit, explicit/scoped tags, and
  the proposed `BACNET_ENABLE_LOGGING` flags.
- #24: scan job state representation and the rule that WAGO ranges remain demo
  configuration only.
- #25: callback lifetime/cancellation and the future COV migration path.

No issue needs an immediate cosmetic update before implementation starts.
