# Client API

This page summarizes the current public client API surface. It documents implemented methods only.

## Core Entry Points

### `BacnetClient`

Current public responsibilities:

- transport lifecycle with `begin()` and `end()`
- client state queries with `isRunning()` and `localPort()`
- reusable logging through `logger()`
- Who-Is sending through `sendWhoIs()`
- I-Am polling through `pollIAm()`
- ReadProperty send/poll helpers through `sendReadProperty()`, `pollReadProperty()`, and `pollReadPropertyStatus()`
- request/response encoding and decoding helpers such as `buildWhoIsRequest()`, `buildReadPropertyRequest()`, `parseIAmResponse()`, and `parseReadPropertyAck()`

### `BacnetDeviceSession`

Current public responsibilities:

- represent one known BACnet/IP target bound to a caller-owned `BacnetClient`
- create a session from known endpoint data through `fromEndpoint()`
- create a session from discovered I-Am metadata through `fromIAm()`
- read one property through `readProperty()`
- read compact object health through `readObjectStatus()`
- collect a property's advertised property list through `readPropertyList()`
- safely attempt all collected properties through `readAllProperties()`
- inspect the latest collected/read property cache through `cachedProperty()`
	and `cachedPropertyCount()`
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
- use the property-list and read-all helpers through `readPropertyList()` and `readAllProperties()`

### `BacnetProperty`

Current public responsibilities:

- describe one object/property/array-index request through `request()`
- perform one property read through `read()`
- create a caller-owned subscription handle through `subscribe()`

## Common Result Types

- `BacnetValue`: typed value holder used by ReadProperty and scan helpers
- `BacnetDeviceSessionReadStatus`: session-level read result status
- `BacnetPropertyReadStatus`: per-property result status for read-all flows
- `BacnetObjectStatus` and `BacnetObjectHealthState`: compact object health view
- `BacnetCachedProperty`: latest cached value, timestamp, status, and error
	metadata for one session/object/property/array-index entry
- `BacnetScannedObject`, `BacnetObjectScanOptions`, `BacnetObjectScanResult`, and `BacnetObjectListScanJob`: object-list scan support types
- `BacnetPropertyReadResult`, `BacnetPropertyListReadResult`, and `BacnetPropertyReadAllResult`: property-list and safe read-all support types
- `BacnetSubscribeOptions`, `BacnetPropertySubscription`, and `BacnetSubscriptionNotification`: fallback-poll subscription support

## Not Implemented

The following are planned or intentionally absent from the current public client runtime:

- real SubscribeCOV client support
- WriteProperty
- priority write helpers
- automatic background polling tasks
- global discovery cache or mandatory object browser state
