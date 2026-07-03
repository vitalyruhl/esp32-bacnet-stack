// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetRemoteObject.h"

BacnetProperty::BacnetProperty(BacnetDeviceSession& session,
                               BacnetObjectId objectId,
                               BacnetPropertyId propertyId,
                               uint32_t arrayIndex)
    : session_(&session),
      objectId_(objectId),
      propertyId_(propertyId),
      arrayIndex_(arrayIndex) {}

BacnetPropertyRequest BacnetProperty::request() const {
  return BacnetPropertyRequest{objectId_, propertyId_, arrayIndex_};
}

BacnetObjectId BacnetProperty::objectId() const {
  return objectId_;
}

BacnetPropertyId BacnetProperty::propertyId() const {
  return propertyId_;
}

uint32_t BacnetProperty::arrayIndex() const {
  return arrayIndex_;
}

BacnetPropertySubscription BacnetProperty::subscribe(
  BacnetSubscriptionCallback callback,
  void* userData,
  const BacnetSubscribeOptions& options) const {
  BacnetPropertySubscription subscription(*session_, objectId_, propertyId_, arrayIndex_, options, callback, userData);
  session_->client().logger().info(
    "BACnet/Subscription", "subscription created %s,%lu %u array=%lu", bacnetObjectTypeText(objectId_.type), static_cast<unsigned long>(objectId_.instance), static_cast<unsigned>(propertyId_), static_cast<unsigned long>(arrayIndex_));
  return subscription;
}

BacnetDeviceSessionReadStatus BacnetProperty::read(
  BacnetValue& value, uint32_t timeoutMs) const {
  return session_->readProperty(objectId_, propertyId_, value, timeoutMs, arrayIndex_);
}

BacnetValue BacnetProperty::lastValue() const {
  BacnetCachedProperty cached;
  if (!session_->cachedProperty(objectId_, propertyId_, cached, arrayIndex_)) {
    return BacnetValue{};
  }
  return cached.value;
}

BacnetPropertyReadStatus BacnetProperty::lastStatus() const {
  BacnetCachedProperty cached;
  if (!session_->cachedProperty(objectId_, propertyId_, cached, arrayIndex_)) {
    return BacnetPropertyReadStatus::Skipped;
  }
  return cached.status;
}

uint32_t BacnetProperty::lastUpdateMs() const {
  BacnetCachedProperty cached;
  if (!session_->cachedProperty(objectId_, propertyId_, cached, arrayIndex_)) {
    return 0;
  }
  return cached.updatedAtMs;
}

BacnetRemoteObject::BacnetRemoteObject(BacnetDeviceSession& session,
                                       BacnetObjectId objectId)
    : session_(&session), objectId_(objectId) {}

BacnetObjectId BacnetRemoteObject::objectId() const {
  return objectId_;
}

BacnetProperty BacnetRemoteObject::property(BacnetPropertyId id,
                                            uint32_t arrayIndex) const {
  return BacnetProperty(*session_, objectId_, id, arrayIndex);
}

BacnetDeviceSessionReadStatus BacnetRemoteObject::readProperty(
  BacnetPropertyId id, BacnetValue& value, uint32_t timeoutMs, uint32_t arrayIndex) const {
  return session_->readProperty(objectId_, id, value, timeoutMs, arrayIndex);
}

BacnetDeviceSessionReadStatus BacnetRemoteObject::readObjectName(
  BacnetValue& value, uint32_t timeoutMs) const {
  return readProperty(BacnetPropertyId::ObjectName, value, timeoutMs);
}

BacnetDeviceSessionReadStatus BacnetRemoteObject::readDescription(
  BacnetValue& value, uint32_t timeoutMs) const {
  return readProperty(BacnetPropertyId::Description, value, timeoutMs);
}

BacnetDeviceSessionReadStatus BacnetRemoteObject::readPresentValue(
  BacnetValue& value, uint32_t timeoutMs) const {
  return readProperty(BacnetPropertyId::PresentValue, value, timeoutMs);
}

BacnetDeviceSessionReadStatus BacnetRemoteObject::readPropertyList(
  BacnetValue& value, uint32_t timeoutMs) const {
  return readProperty(BacnetPropertyId::PropertyList, value, timeoutMs);
}

BacnetPropertyListReadResult BacnetRemoteObject::readPropertyList(
  BacnetPropertyId* properties, size_t propertyCapacity, uint32_t timeoutMs) const {
  return session_->readPropertyList(objectId_, properties, propertyCapacity, timeoutMs);
}

BacnetPropertyReadAllResult BacnetRemoteObject::readAllProperties(
  const BacnetPropertyId* properties, size_t propertyCount, BacnetPropertyReadResult* results, size_t resultCapacity, uint32_t timeoutMs) const {
  return session_->readAllProperties(objectId_, properties, propertyCount, results, resultCapacity, timeoutMs);
}

BacnetPropertyReadAllResult BacnetRemoteObject::readAllAdvertisedProperties(
  BacnetPropertyId* properties,
  size_t propertyCapacity,
  BacnetPropertyReadResult* results,
  size_t resultCapacity,
  uint32_t timeoutMs) const {
  return session_->readAllAdvertisedProperties(
    objectId_, properties, propertyCapacity, results, resultCapacity, timeoutMs);
}
