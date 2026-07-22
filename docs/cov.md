# Change of Value (COV)

This page is the canonical description of the implemented BACnet/IP COV
subset. It applies to the portable client/session API and the portable
`BacnetServer` runtime. It does not describe a complete BACnet COV or event
reporting implementation.

## Supported subscription forms

- `SubscribeCOV` is an object-level subscription. The server profile sends
  `Present_Value` and `Status_Flags` together for that object.
- `SubscribeCOVProperty` is a property-level subscription. The server profile
  sends only the requested property and array index.
- A property-level subscription for a real-valued `Present_Value` may carry
  `COV_Increment`. The server rejects an increment for another property or a
  non-real value. Changes smaller than the accepted increment do not notify.

The server accepts one bounded, allocation-free table of eight subscriptions.
An entry is identified by the subscriber endpoint, Process Identifier,
monitored object, subscription form, property, and array index. Renewing the
same tuple updates the existing entry rather than allocating another one.

## Client subscription API

The high-level API is `BacnetProperty::subscribe()` from
`BacnetRemoteObject.h`. Keep its returned `BacnetPropertySubscription` alive
and drive it from the application loop with `BacnetDeviceSession::poll()`.

```cpp
#include <BacnetRemoteObject.h>

BacnetSubscribeOptions options{};
options.preferCov = true;
options.usePropertyCov = true;
options.covLifetimeSeconds = 60;
options.covRenewBeforeSeconds = 5;
options.hasCovIncrement = true;
options.covIncrement = 0.5F;

auto light = session.object(BacnetObjectId{
  static_cast<uint16_t>(BacnetObjectType::AnalogInput), 0});
auto subscription = light.property(BacnetPropertyId::PresentValue).subscribe(
  onSubscriptionNotification, nullptr, options);

// Call from loop(); it does not start a background task.
session.poll(subscription);
```

Set `usePropertyCov` to `false` for object-level `SubscribeCOV`. The
subscription handle exposes `covStatus()`, `covRejectReason()`, cached value
and status, and the most recent notification reason. A notification callback
runs synchronously while `BacnetDeviceSession::poll()` is executing; copy a
value before retaining it.

`BacnetClient::sendSubscribeCov()` and
`BacnetClient::sendSubscribeCovProperty()` are the lower-level request APIs.
Their caller supplies the destination, Process Identifier, monitored object,
lifetime, and Invoke ID; the property variant additionally supplies property,
array index, and optional increment. Use
`pollSubscribeCov()` or `pollSubscribeCovProperty()` with the same Invoke ID
to receive the registration result.

## Notifications and acknowledgement

Set `BacnetSubscribeOptions::issueConfirmedNotifications` to request confirmed
notifications. `BacnetClient::pollCovNotification()` automatically returns a
`SimpleACK` for a confirmed COV notification before exposing it. Applications
using the session API obtain the same behavior by continuing to call
`BacnetDeviceSession::poll()`; they do not send a separate acknowledgement.

After accepting a subscription, the server invalidates its prior COV snapshot.
The next server `poll()` therefore sends an initial notification. Later
notifications are emitted when the subscribed value changes; object-level COV
also reports a `Status_Flags` change. The server keeps a confirmed notification
pending until its `SimpleACK` arrives or its configured retry count is used.

## Lifetime, renewal, and cancellation

`BacnetSubscribeOptions::covLifetimeSeconds` defaults to 60 seconds. For a
successful high-level COV registration, the session schedules renewal before
expiry by `covRenewBeforeSeconds` (default 5 seconds). An acknowledged renewal
returns the handle to `BacnetCovSubscriptionStatus::Active`.

The portable server needs `BacnetServer::setClock()` for timed expiration and
confirmed-notification retry timing. With a clock, a positive lifetime expires
at the requested deadline. A request with `lifetimeSeconds == 0` cancels the
matching server entry. Send that cancellation explicitly through the matching
low-level `sendSubscribeCov()` or `sendSubscribeCovProperty()` call, preserving
its Process Identifier, monitored object, and property tuple.

`BacnetPropertySubscription::stop()` stops the local handle only; it does not
send a remote cancellation. Destruction has the same local-only effect.

## Retry, fallback, and recovery

The client subscription abstraction has two independent paths:

- When a SubscribeCOV response is an Error, Reject, Abort, or timeout, it sets
  the handle to the corresponding COV status, uses the configured polling
  fallback, and schedules the next COV registration after
  `covRenewBeforeSeconds` (or one second when that setting is zero).
- `fallbackPollMs = 0` disables periodic fallback polls. An initial refresh
  may still be requested by the handle's `immediateFirstRead` setting.
- A later acknowledged COV registration clears fallback state and restores
  `Active`. Discovery and session rebinding remain application-owned; the
  client does not maintain a global device registry.

For confirmed server notifications, retries after a missing `SimpleACK` are
bounded by `BacnetServerDevice::numberOfApduRetries` and its `apduTimeout`.
After that retry limit, the server returns the subscription to active state.

This is not a universal transport-retry guarantee: a local failure while
sending a SubscribeCOV request immediately enters polling fallback, but the
current implementation does not apply the delayed renewal interval to that
specific send-failure path. Similarly, an outbound server notification send
failure is reported through the COV diagnostic listener and is retried on a
subsequent server poll if its value remains changed. Applications that need a
strict transport request-rate bound must supervise transport availability.

## Server API and diagnostics

`BacnetServer::poll()` receives subscription requests, manages lifetime and
notifications, and must be called regularly by the application. The following
public diagnostics are intended for short, local observation:

- `covSubscriptionCount()` reports active table entries.
- `covSubscriptionAt()` copies one read-only subscription view, including peer,
  Process Identifier, object/property tuple, lifetime, state, and confirmation
  fields.
- `setCovDiagnosticListener()` reports activation, detected change, send, and
  send-failure events synchronously during `poll()`.

The table and diagnostics are not a persistence or authentication mechanism.
Client routing uses the COV monitored object/property information; do not use
this subset as an authenticated source-verification layer.

## Scope boundary

The implementation covers the client/session flow and the small read-only
server profile used by the examples. It is not a substitute for full BACnet
event reporting, intrinsic reporting, arbitrary-object COV support, or a
network-wide interoperability certification. See the paired
[ESP-to-ESP examples](../examples/hil-cov-espClient-to-espServer-acceptance/README.md)
for the focused hardware acceptance scope.
