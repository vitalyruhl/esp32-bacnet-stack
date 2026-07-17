# Client API

This page summarizes the current public client API surface. It documents implemented methods only.

## Core Entry Points

### `BacnetClient`

Current public responsibilities:

- use a caller-provided `BacnetDatagramTransport` and optional
  `BacnetMonotonicClock`
- transport lifecycle with `begin()` and `end()`
- client state queries with `isRunning()` and `localPort()`
- reusable logging through `logger()`
- Who-Is sending through `sendWhoIs()`
- I-Am polling through `pollIAm()`
- ReadProperty send/poll helpers through `sendReadProperty()`, `pollReadProperty()`, and `pollReadPropertyStatus()`
- explicit WriteProperty send/poll helpers through `sendWriteProperty()` and
  `pollWriteProperty()` when `ESP_BACNET_ENABLE_WRITE_PROPERTY=1`
- request/response encoding and decoding helpers such as `buildWhoIsRequest()`,
  `buildReadPropertyRequest()`, `buildWritePropertyRequest()`,
  `parseIAmResponse()`, and `parseReadPropertyAck()`

### `BacnetDeviceSession`

Current public responsibilities:

- represent one known BACnet/IP target bound to a caller-owned `BacnetClient`
- create a session from known endpoint data through `fromEndpoint()`
- create a session from discovered I-Am metadata through `fromIAm()`, retaining
  the observed source port unless an explicit override is supplied
- read one property through `readProperty()`
- explicitly write one typed property through `writeProperty()` when enabled
  and optionally provide `BacnetWritePropertyOptions` for an array index and a
  BACnet priority from `1` through `16`; priority requests additionally require
  `ESP_BACNET_ENABLE_PRIORITY_WRITE=1`
- read complete `priority-array` values through the typed
  `BacnetPriorityArray` helper, or read one priority slot through the scalar
  indexed helper; read `relinquish-default` through the detailed
  `BacnetPropertyReadStatus` convenience helper; write or relinquish
  `present-value` explicitly through `writePresentValue()` and
  `relinquishPresentValue()`; run a strict command-priority reset through
  `relinquishAllPriorities()` or explicitly skip Minimum On/Off priority 6
  with `BacnetPriorityResetOptions` when both write feature gates are enabled;
  inspect its
  `BacnetPriorityRelinquishResult`
- read compact object health through `readObjectStatus()`
- collect a property's advertised property list through `readPropertyList()`
- safely attempt all collected properties through `readAllProperties()`
- use `readAllAdvertisedProperties()` to read the advertised property list and
  then attempt every collected property without aborting on one property failure
- inspect the latest collected/read property cache through `cachedProperty()`
	and `cachedPropertyCount()`
- inspect whether a session-owned scan, subscription, or non-blocking property
  read is in flight through `isBusy()`; advance only existing work through
  `pollInFlight()` or a caller-owned `BacnetPropertyReadJob`
- create `BacnetRemoteObject` wrappers through `object()`
- run blocking object-list scans through `scanObjectList()`
- run non-blocking object-list scans through `beginObjectListScan()`, `pollObjectListScan()`, and `cancelObjectListScan()`
- drive property subscriptions through `poll()`

### `BacnetRemoteObject`

Current public responsibilities:

- wrap one object on a `BacnetDeviceSession`
- create one `BacnetProperty` wrapper through `property()`
- read one property through `readProperty()`
- read convenience properties through `readObjectName()`, `readDescription()`, `readPresentValue()`, and `readPropertyList()`
- use the property-list and read-all helpers through `readPropertyList()`, `readAllProperties()`, and `readAllAdvertisedProperties()`

### `BacnetProperty`

Current public responsibilities:

- describe one object/property/array-index request through `request()`
- perform one property read through `read()`
- inspect the cached read state through `lastValue()`, `lastStatus()`, and
	`lastUpdateMs()`
- create a caller-owned subscription handle through `subscribe()`

## Common Result Types

- `BacnetValue`: typed value holder used by scalar ReadProperty and scan helpers;
  decoded BACnet errors retain canonical text such as
  `unknown-object (object/31)`
- `bacnetErrorClassName()` and `bacnetErrorCodeName()`: canonical portable
  BACnet error-name mappings used by protocol decoding and presentation layers
- `BacnetPriorityArray`: complete 16-slot typed `priority-array` result;
  `slots[0]` through `slots[15]` correspond to BACnet priorities `1` through
  `16`
- `BacnetPriorityResetOptions` and `BacnetPriorityRelinquishResult`: explicit
  strict or writable command-priority reset behavior and per-reset completed,
  skipped, and failed priority reporting; see
  [Command priority reset semantics](../bacnet-command-priority.md)
- `BacnetDeviceSessionWriteStatus` and `BacnetWritePropertyPollStatus`: typed
  WriteProperty result states
- `BacnetDeviceSessionReadStatus`: session-level read result status
- `BacnetPropertyReadStatus`: per-property result status for read-all flows
- `BacnetObjectStatus` and `BacnetObjectHealthState`: compact object health view
- `BacnetCachedProperty`: latest cached value, timestamp, status, and error
	metadata for one session/object/property/array-index entry
- `BacnetScannedObject`, `BacnetObjectScanOptions`, `BacnetObjectScanResult`, and `BacnetObjectListScanJob`: object-list scan support types
- `BacnetPropertyReadResult`, `BacnetPropertyListReadResult`, and `BacnetPropertyReadAllResult`: property-list and safe read-all support types
- `BacnetSubscribeOptions`, `BacnetPropertySubscription`, and `BacnetSubscriptionNotification`: SubscribeCOV with polling-fallback subscription support

## Not Implemented

The following are planned or intentionally absent from the current public client runtime:

- automatic background polling tasks
- global discovery cache or mandatory object browser state
