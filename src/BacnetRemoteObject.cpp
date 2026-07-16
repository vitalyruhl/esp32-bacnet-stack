// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetRemoteObject.h"

namespace {

bool enumValueFromCachedProcessValue(const BacnetValue& value,
                                     uint32_t& output) {
  if (value.type == BacnetValueType::Enumerated ||
      value.type == BacnetValueType::Unsigned) {
    output = value.unsignedValue;
    return true;
  }
  return false;
}

bool booleanValueFromCachedProcessValue(const BacnetValue& value,
                                        bool& output) {
  if (value.type == BacnetValueType::Boolean) {
    output = value.booleanValue;
    return true;
  }
  return false;
}

BacnetPropertyReadStatus cachedPropertyStatus(const BacnetDeviceSession& session,
                                              BacnetObjectId objectId,
                                              BacnetPropertyId propertyId,
                                              BacnetValue* value = nullptr) {
  BacnetCachedProperty cached;
  if (!session.cachedProperty(objectId, propertyId, cached)) {
    return BacnetPropertyReadStatus::Skipped;
  }
  if (value != nullptr) {
    *value = cached.value;
  }
  return cached.status;
}

} // namespace

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
  session_->client().logger().debug(
    "BACnet/Subscription", "subscription created %s,%lu %u array=%lu", bacnetObjectTypeText(objectId_.type), static_cast<unsigned long>(objectId_.instance), static_cast<unsigned>(propertyId_), static_cast<unsigned long>(arrayIndex_));
  return subscription;
}

BacnetDeviceSessionReadStatus BacnetProperty::read(
  BacnetValue& value, uint32_t timeoutMs) const {
  return session_->readProperty(objectId_, propertyId_, value, timeoutMs, arrayIndex_);
}

bool BacnetProperty::hasValue() const {
  BacnetCachedProperty cached;
  if (!session_->cachedProperty(objectId_, propertyId_, cached, arrayIndex_)) {
    return false;
  }
  return cached.hasValue;
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
  return lastSuccessMs();
}

uint32_t BacnetProperty::lastAttemptMs() const {
  BacnetCachedProperty cached;
  if (!session_->cachedProperty(objectId_, propertyId_, cached, arrayIndex_)) {
    return 0;
  }
  return cached.lastAttemptMs;
}

uint32_t BacnetProperty::lastSuccessMs() const {
  BacnetCachedProperty cached;
  if (!session_->cachedProperty(objectId_, propertyId_, cached, arrayIndex_)) {
    return 0;
  }
  return cached.lastSuccessMs;
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

BacnetPropertyReadStatus BacnetRemoteObject::readPriorityArray(
  BacnetValue& value, uint32_t timeoutMs, uint32_t arrayIndex) const {
  return session_->readPropertyStatus(objectId_, BacnetPropertyId::PriorityArray, value, timeoutMs, arrayIndex);
}

BacnetPropertyReadStatus BacnetRemoteObject::readPriorityArray(
  BacnetPriorityArray& value, uint32_t timeoutMs) const {
  return session_->readPriorityArray(objectId_, value, timeoutMs);
}

BacnetPropertyReadStatus BacnetRemoteObject::readRelinquishDefault(
  BacnetValue& value, uint32_t timeoutMs) const {
  return session_->readPropertyStatus(objectId_,
                                      BacnetPropertyId::RelinquishDefault,
                                      value,
                                      timeoutMs);
}

BacnetDeviceSessionWriteStatus BacnetRemoteObject::writePresentValue(
  const BacnetValue& value, uint8_t priority, uint32_t timeoutMs) const {
  BacnetWritePropertyOptions options;
  options.hasPriority = true;
  options.priority = priority;
  return session_->writeProperty(static_cast<BacnetObjectType>(objectId_.type),
                                 objectId_.instance,
                                 BacnetPropertyId::PresentValue,
                                 value,
                                 options,
                                 timeoutMs);
}

BacnetDeviceSessionWriteStatus BacnetRemoteObject::relinquishPresentValue(
  uint8_t priority, uint32_t timeoutMs) const {
  BacnetValue value;
  value.type = BacnetValueType::Null;
  return writePresentValue(value, priority, timeoutMs);
}

BacnetPriorityRelinquishResult BacnetRemoteObject::relinquishAllPriorities(
  uint32_t timeoutMs) const {
  BacnetPriorityRelinquishResult result;
  for (uint8_t priority = 1; priority <= 16; ++priority) {
    result.status = relinquishPresentValue(priority, timeoutMs);
    if (result.status != BacnetDeviceSessionWriteStatus::Ack) {
      result.failedPriority = priority;
      return result;
    }
    result.completedPriorities = priority;
  }
  return result;
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

BacnetProcessObject::BacnetProcessObject(BacnetDeviceSession& session,
                                         BacnetObjectType objectType,
                                         uint32_t objectInstance)
    : BacnetProcessObject(
        session,
        BacnetObjectId{static_cast<uint16_t>(objectType), objectInstance}) {}

BacnetProcessObject::BacnetProcessObject(BacnetDeviceSession& session,
                                         BacnetObjectId objectId)
    : session_(&session), objectId_(objectId) {}

BacnetObjectId BacnetProcessObject::objectId() const {
  return objectId_;
}

BacnetRemoteObject BacnetProcessObject::remoteObject() const {
  return session_->object(objectId_);
}

BacnetProperty BacnetProcessObject::property(BacnetPropertyId id,
                                             uint32_t arrayIndex) const {
  return remoteObject().property(id, arrayIndex);
}

BacnetProperty BacnetProcessObject::presentValueProperty() const {
  return property(BacnetPropertyId::PresentValue);
}

BacnetDeviceSessionReadStatus BacnetProcessObject::readPresentValue(
  BacnetValue& value, uint32_t timeoutMs) const {
  return remoteObject().readPresentValue(value, timeoutMs);
}

BacnetDeviceSessionReadStatus BacnetProcessObject::readPresentValue(
  uint32_t timeoutMs) const {
  BacnetValue value;
  return readPresentValue(value, timeoutMs);
}

BacnetValue BacnetProcessObject::presentValue() const {
  return presentValueProperty().lastValue();
}

bool BacnetProcessObject::hasPresentValue() const {
  return presentValueProperty().hasValue();
}

BacnetPropertyReadStatus BacnetProcessObject::presentValueStatus() const {
  return presentValueProperty().lastStatus();
}

uint32_t BacnetProcessObject::presentValueLastUpdateMs() const {
  return presentValueProperty().lastUpdateMs();
}

uint32_t BacnetProcessObject::presentValueLastAttemptMs() const {
  return presentValueProperty().lastAttemptMs();
}

uint32_t BacnetProcessObject::presentValueLastSuccessMs() const {
  return presentValueProperty().lastSuccessMs();
}

BacnetDeviceSessionReadStatus BacnetProcessObject::readEngineeringUnits(
  BacnetValue& value, uint32_t timeoutMs) const {
  return property(BacnetPropertyId::Units).read(value, timeoutMs);
}

BacnetDeviceSessionReadStatus BacnetProcessObject::readEngineeringUnits(
  uint32_t timeoutMs) const {
  BacnetValue value;
  return readEngineeringUnits(value, timeoutMs);
}

BacnetValue BacnetProcessObject::engineeringUnits() const {
  return property(BacnetPropertyId::Units).lastValue();
}

const char* BacnetProcessObject::engineeringUnitSymbol() const {
  uint32_t unitId = 0;
  if (!bacnetEngineeringUnitId(engineeringUnits(), unitId)) {
    return nullptr;
  }
  return bacnetEngineeringUnitSymbol(unitId);
}

BacnetDeviceSessionReadStatus BacnetProcessObject::readMinPresentValue(
  BacnetValue& value, uint32_t timeoutMs) const {
  return property(BacnetPropertyId::MinPresentValue).read(value, timeoutMs);
}

BacnetDeviceSessionReadStatus BacnetProcessObject::readMinPresentValue(
  uint32_t timeoutMs) const {
  BacnetValue value;
  return readMinPresentValue(value, timeoutMs);
}

BacnetDeviceSessionReadStatus BacnetProcessObject::readMaxPresentValue(
  BacnetValue& value, uint32_t timeoutMs) const {
  return property(BacnetPropertyId::MaxPresentValue).read(value, timeoutMs);
}

BacnetDeviceSessionReadStatus BacnetProcessObject::readMaxPresentValue(
  uint32_t timeoutMs) const {
  BacnetValue value;
  return readMaxPresentValue(value, timeoutMs);
}

BacnetDeviceSessionReadStatus BacnetProcessObject::readResolution(
  BacnetValue& value, uint32_t timeoutMs) const {
  return property(BacnetPropertyId::Resolution).read(value, timeoutMs);
}

BacnetDeviceSessionReadStatus BacnetProcessObject::readResolution(
  uint32_t timeoutMs) const {
  BacnetValue value;
  return readResolution(value, timeoutMs);
}

BacnetDeviceSessionReadStatus BacnetProcessObject::readCovIncrement(
  BacnetValue& value, uint32_t timeoutMs) const {
  return property(BacnetPropertyId::CovIncrement).read(value, timeoutMs);
}

BacnetDeviceSessionReadStatus BacnetProcessObject::readCovIncrement(
  uint32_t timeoutMs) const {
  BacnetValue value;
  return readCovIncrement(value, timeoutMs);
}

BacnetObjectStatus BacnetProcessObject::readStatus(
  uint32_t timeoutMs, bool presentValueRequired) const {
  BacnetObjectStatus status;
  status.objectId = objectId_;

  const BacnetProperty presentValueProp = presentValueProperty();
  presentValueProp.read(status.presentValue, timeoutMs);
  status.presentValueStatus = presentValueProp.lastStatus();

  BacnetValue statusFlagsValue;
  const BacnetProperty statusFlagsProperty =
    property(BacnetPropertyId::StatusFlags);
  statusFlagsProperty.read(statusFlagsValue, timeoutMs);
  status.statusFlagsStatus = statusFlagsProperty.lastStatus();
  if (status.statusFlagsStatus == BacnetPropertyReadStatus::Ack &&
      !bacnetDecodeStatusFlags(statusFlagsValue, status.statusFlags)) {
    status.statusFlagsStatus = BacnetPropertyReadStatus::DecodeError;
  }

  BacnetValue eventStateValue;
  const BacnetProperty eventStateProperty =
    property(BacnetPropertyId::EventState);
  eventStateProperty.read(eventStateValue, timeoutMs);
  status.eventStateStatus = eventStateProperty.lastStatus();
  if (status.eventStateStatus == BacnetPropertyReadStatus::Ack &&
      !enumValueFromCachedProcessValue(eventStateValue, status.eventState)) {
    status.eventStateStatus = BacnetPropertyReadStatus::DecodeError;
  }

  BacnetValue reliabilityValue;
  const BacnetProperty reliabilityProperty =
    property(BacnetPropertyId::Reliability);
  reliabilityProperty.read(reliabilityValue, timeoutMs);
  status.reliabilityStatus = reliabilityProperty.lastStatus();
  if (status.reliabilityStatus == BacnetPropertyReadStatus::Ack &&
      !enumValueFromCachedProcessValue(reliabilityValue, status.reliability)) {
    status.reliabilityStatus = BacnetPropertyReadStatus::DecodeError;
  }

  BacnetValue outOfServiceValue;
  const BacnetProperty outOfServiceProperty =
    property(BacnetPropertyId::OutOfService);
  outOfServiceProperty.read(outOfServiceValue, timeoutMs);
  status.outOfServiceStatus = outOfServiceProperty.lastStatus();
  if (status.outOfServiceStatus == BacnetPropertyReadStatus::Ack &&
      !booleanValueFromCachedProcessValue(outOfServiceValue,
                                          status.outOfService)) {
    status.outOfServiceStatus = BacnetPropertyReadStatus::DecodeError;
  }

  status.state = bacnetDeriveObjectHealthState(status, presentValueRequired);
  return status;
}

BacnetStatusFlags BacnetProcessObject::statusFlags() const {
  BacnetValue value;
  const BacnetPropertyReadStatus status = cachedPropertyStatus(
    *session_, objectId_, BacnetPropertyId::StatusFlags, &value);
  BacnetStatusFlags flags;
  if (status == BacnetPropertyReadStatus::Ack) {
    bacnetDecodeStatusFlags(value, flags);
  }
  return flags;
}

BacnetPropertyReadStatus BacnetProcessObject::statusFlagsStatus() const {
  BacnetValue value;
  const BacnetPropertyReadStatus status = cachedPropertyStatus(
    *session_, objectId_, BacnetPropertyId::StatusFlags, &value);
  if (status == BacnetPropertyReadStatus::Ack) {
    BacnetStatusFlags flags;
    return bacnetDecodeStatusFlags(value, flags)
             ? BacnetPropertyReadStatus::Ack
             : BacnetPropertyReadStatus::DecodeError;
  }
  return status;
}

uint32_t BacnetProcessObject::eventState() const {
  BacnetValue value;
  uint32_t eventStateValue = 0;
  if (cachedPropertyStatus(*session_, objectId_, BacnetPropertyId::EventState, &value) ==
      BacnetPropertyReadStatus::Ack) {
    enumValueFromCachedProcessValue(value, eventStateValue);
  }
  return eventStateValue;
}

BacnetPropertyReadStatus BacnetProcessObject::eventStateStatus() const {
  BacnetValue value;
  const BacnetPropertyReadStatus status = cachedPropertyStatus(
    *session_, objectId_, BacnetPropertyId::EventState, &value);
  if (status == BacnetPropertyReadStatus::Ack) {
    uint32_t eventStateValue = 0;
    return enumValueFromCachedProcessValue(value, eventStateValue)
             ? BacnetPropertyReadStatus::Ack
             : BacnetPropertyReadStatus::DecodeError;
  }
  return status;
}

uint32_t BacnetProcessObject::reliability() const {
  BacnetValue value;
  uint32_t reliabilityValue = 0;
  if (cachedPropertyStatus(*session_, objectId_, BacnetPropertyId::Reliability, &value) ==
      BacnetPropertyReadStatus::Ack) {
    enumValueFromCachedProcessValue(value, reliabilityValue);
  }
  return reliabilityValue;
}

BacnetPropertyReadStatus BacnetProcessObject::reliabilityStatus() const {
  BacnetValue value;
  const BacnetPropertyReadStatus status = cachedPropertyStatus(
    *session_, objectId_, BacnetPropertyId::Reliability, &value);
  if (status == BacnetPropertyReadStatus::Ack) {
    uint32_t reliabilityValue = 0;
    return enumValueFromCachedProcessValue(value, reliabilityValue)
             ? BacnetPropertyReadStatus::Ack
             : BacnetPropertyReadStatus::DecodeError;
  }
  return status;
}

bool BacnetProcessObject::outOfService() const {
  BacnetValue value;
  bool isOutOfService = false;
  if (cachedPropertyStatus(*session_, objectId_, BacnetPropertyId::OutOfService, &value) ==
      BacnetPropertyReadStatus::Ack) {
    booleanValueFromCachedProcessValue(value, isOutOfService);
  }
  return isOutOfService;
}

BacnetPropertyReadStatus BacnetProcessObject::outOfServiceStatus() const {
  BacnetValue value;
  const BacnetPropertyReadStatus status = cachedPropertyStatus(
    *session_, objectId_, BacnetPropertyId::OutOfService, &value);
  if (status == BacnetPropertyReadStatus::Ack) {
    bool isOutOfService = false;
    return booleanValueFromCachedProcessValue(value, isOutOfService)
             ? BacnetPropertyReadStatus::Ack
             : BacnetPropertyReadStatus::DecodeError;
  }
  return status;
}
