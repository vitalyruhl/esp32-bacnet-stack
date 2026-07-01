// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetDeviceSession.h"

#include "BacnetRemoteObject.h"

#include <cstring>

namespace {

bool objectIdFromObjectListValue(const BacnetValue& value,
                                 BacnetObjectId& objectId) {
  if (value.type != BacnetValueType::ObjectIdentifier) {
    return false;
  }
  objectId = value.objectValue;
  return true;
}

bool textEquals(const char* left, const char* right) {
  const char* resolvedLeft = left != nullptr ? left : "";
  const char* resolvedRight = right != nullptr ? right : "";
  return std::strcmp(resolvedLeft, resolvedRight) == 0;
}

}  // namespace

BacnetPropertySubscription::BacnetPropertySubscription(
    BacnetDeviceSession& session,
    BacnetObjectId objectId,
    BacnetPropertyId propertyId,
    uint32_t arrayIndex,
    BacnetSubscribeOptions options,
    BacnetSubscriptionCallback callback,
    void* userData)
    : session_(&session),
      objectId_(objectId),
      propertyId_(propertyId),
      arrayIndex_(arrayIndex),
      options_(options),
      callback_(callback),
      userData_(userData),
      initialReadPending_(options_.immediateFirstRead) {
  if (options_.timeoutMs == 0) {
    options_.timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs;
  }
  if (options_.fallbackPollMs > 0) {
    nextPollAtMs_ = millis() + options_.fallbackPollMs;
  }
}

BacnetPropertySubscription::BacnetPropertySubscription(
    BacnetPropertySubscription&& other) noexcept {
  moveFrom(other);
}

BacnetPropertySubscription& BacnetPropertySubscription::operator=(
    BacnetPropertySubscription&& other) noexcept {
  if (this != &other) {
    if (session_ != nullptr) {
      session_->releaseSubscription(*this);
    }
    moveFrom(other);
  }
  return *this;
}

BacnetPropertySubscription::~BacnetPropertySubscription() {
  if (session_ != nullptr) {
    session_->releaseSubscription(*this);
  }
  active_ = false;
  refreshRequested_ = false;
  initialReadPending_ = false;
  clearInFlightState();
}

BacnetObjectId BacnetPropertySubscription::objectId() const {
  return objectId_;
}

BacnetPropertyId BacnetPropertySubscription::propertyId() const {
  return propertyId_;
}

uint32_t BacnetPropertySubscription::arrayIndex() const {
  return arrayIndex_;
}

bool BacnetPropertySubscription::active() const {
  return active_;
}

bool BacnetPropertySubscription::inFlight() const {
  return inFlight_;
}

bool BacnetPropertySubscription::hasValue() const {
  return hasValue_;
}

const BacnetValue& BacnetPropertySubscription::lastValue() const {
  return lastValue_;
}

BacnetDeviceSessionReadStatus BacnetPropertySubscription::lastStatus() const {
  return lastStatus_;
}

uint32_t BacnetPropertySubscription::lastUpdateMs() const {
  return lastUpdateMs_;
}

BacnetSubscriptionNotificationReason
BacnetPropertySubscription::lastNotificationReason() const {
  return lastNotificationReason_;
}

void BacnetPropertySubscription::stop() {
  if (!active_) {
    return;
  }

  active_ = false;
  refreshRequested_ = false;
  initialReadPending_ = false;

  if (session_ != nullptr) {
    session_->client().logger().info(
        "BACnet/Subscription", "subscription stopped %s,%lu %u array=%lu",
        bacnetObjectTypeText(objectId_.type),
        static_cast<unsigned long>(objectId_.instance),
        static_cast<unsigned>(propertyId_), static_cast<unsigned long>(arrayIndex_));
    if (session_->inFlightSubscription_ == this) {
      session_->inFlightSubscription_ = nullptr;
    }
  }

  clearInFlightState();
}

void BacnetPropertySubscription::requestRefresh() {
  if (!active_) {
    return;
  }
  refreshRequested_ = true;
}

bool BacnetPropertySubscription::isDue(uint32_t nowMs) const {
  if (!active_ || inFlight_) {
    return false;
  }
  if (refreshRequested_ || initialReadPending_) {
    return true;
  }
  if (options_.fallbackPollMs == 0) {
    return false;
  }
  return static_cast<int32_t>(nowMs - nextPollAtMs_) >= 0;
}

void BacnetPropertySubscription::scheduleNextPoll(uint32_t nowMs) {
  if (options_.fallbackPollMs == 0) {
    nextPollAtMs_ = 0;
    return;
  }
  nextPollAtMs_ = nowMs + options_.fallbackPollMs;
}

void BacnetPropertySubscription::clearInFlightState() {
  inFlight_ = false;
  inFlightInvokeId_ = 0;
  inFlightStartedAt_ = 0;
  inFlightTrigger_ = PollTrigger::None;
}

void BacnetPropertySubscription::moveFrom(BacnetPropertySubscription& other) {
  session_ = other.session_;
  objectId_ = other.objectId_;
  propertyId_ = other.propertyId_;
  arrayIndex_ = other.arrayIndex_;
  options_ = other.options_;
  callback_ = other.callback_;
  userData_ = other.userData_;
  active_ = other.active_;
  inFlight_ = other.inFlight_;
  hasValue_ = other.hasValue_;
  refreshRequested_ = other.refreshRequested_;
  initialReadPending_ = other.initialReadPending_;
  hasTerminalStatus_ = other.hasTerminalStatus_;
  lastValue_ = other.lastValue_;
  lastStatus_ = other.lastStatus_;
  lastUpdateMs_ = other.lastUpdateMs_;
  nextPollAtMs_ = other.nextPollAtMs_;
  lastNotificationReason_ = other.lastNotificationReason_;
  inFlightInvokeId_ = other.inFlightInvokeId_;
  inFlightStartedAt_ = other.inFlightStartedAt_;
  inFlightTrigger_ = other.inFlightTrigger_;

  if (session_ != nullptr && session_->inFlightSubscription_ == &other) {
    session_->inFlightSubscription_ = this;
  }

  other.session_ = nullptr;
  other.active_ = false;
  other.clearInFlightState();
  other.hasValue_ = false;
  other.refreshRequested_ = false;
  other.initialReadPending_ = false;
  other.hasTerminalStatus_ = false;
  other.lastStatus_ = BacnetDeviceSessionReadStatus::Skipped;
  other.lastUpdateMs_ = 0;
  other.nextPollAtMs_ = 0;
  other.lastNotificationReason_ = BacnetSubscriptionNotificationReason::None;
  other.callback_ = nullptr;
  other.userData_ = nullptr;
}

BacnetDeviceSession::BacnetDeviceSession(BacnetClient& client,
                                         uint32_t deviceInstance,
                                         IPAddress address, uint16_t port)
    : client_(client),
      deviceInstance_(deviceInstance),
      address_(address),
      port_(port) {}

BacnetClient& BacnetDeviceSession::client() {
  return client_;
}

const BacnetClient& BacnetDeviceSession::client() const {
  return client_;
}

uint32_t BacnetDeviceSession::deviceInstance() const {
  return deviceInstance_;
}

IPAddress BacnetDeviceSession::address() const {
  return address_;
}

uint16_t BacnetDeviceSession::port() const {
  return port_;
}

BacnetObjectId BacnetDeviceSession::deviceObject() const {
  return BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::Device),
                        deviceInstance_};
}

BacnetRemoteObject BacnetDeviceSession::object(BacnetObjectId objectId) {
  return BacnetRemoteObject(*this, objectId);
}

BacnetRemoteObject BacnetDeviceSession::object(BacnetObjectType objectType,
                                               uint32_t objectInstance) {
  return object(
      BacnetObjectId{static_cast<uint16_t>(objectType), objectInstance});
}

BacnetDeviceSessionReadStatus BacnetDeviceSession::readProperty(
    BacnetObjectId object, BacnetPropertyId property, BacnetValue& value,
    uint32_t timeoutMs, uint32_t arrayIndex) {
  value = BacnetValue{};
  const BacnetPropertyRequest request{object, property, arrayIndex};
  if (inFlightSubscription_ != nullptr) {
    client_.logger().warn(
        "BACnet/ReadProperty",
        "ReadProperty %s,%lu property=%lu skipped: session subscription busy",
        bacnetObjectTypeText(object.type),
        static_cast<unsigned long>(object.instance),
        static_cast<unsigned long>(property));
    return BacnetDeviceSessionReadStatus::Busy;
  }

  const uint8_t invokeId = allocateInvokeId();

  if (!client_.sendReadProperty(address_, request, invokeId, port_)) {
    return BacnetDeviceSessionReadStatus::SendFailed;
  }

  const unsigned long startedAt = millis();
  while (true) {
    const BacnetReadPropertyPollStatus status =
        client_.pollReadPropertyStatus(value, invokeId, request);
    if (status == BacnetReadPropertyPollStatus::Ack) {
      return BacnetDeviceSessionReadStatus::Ack;
    }
    if (status == BacnetReadPropertyPollStatus::Error) {
      return BacnetDeviceSessionReadStatus::Error;
    }
    if (millis() - startedAt >= timeoutMs) {
      client_.logReadPropertyTimeout(invokeId, request);
      return BacnetDeviceSessionReadStatus::Timeout;
    }
    yield();
  }
}

BacnetDeviceSessionReadStatus BacnetDeviceSession::readProperty(
    BacnetObjectType objectType, uint32_t objectInstance,
    BacnetPropertyId property, BacnetValue& value, uint32_t timeoutMs,
    uint32_t arrayIndex) {
  return readProperty(
      BacnetObjectId{static_cast<uint16_t>(objectType), objectInstance},
      property, value, timeoutMs, arrayIndex);
}

BacnetObjectScanResult BacnetDeviceSession::scanObjectList(
    const BacnetObjectScanOptions& options,
    BacnetScannedObject* results,
    size_t resultCapacity) {
  BacnetLogger& logger = client_.logger();
  logger.info(
      "BACnet/Scan",
      "scan start device %lu target %u.%u.%u.%u:%u max-entries %lu filter-count "
      "%u",
      static_cast<unsigned long>(deviceInstance_),
      static_cast<unsigned>(address_[0]),
      static_cast<unsigned>(address_[1]),
      static_cast<unsigned>(address_[2]),
      static_cast<unsigned>(address_[3]),
      static_cast<unsigned>(port_),
      static_cast<unsigned long>(options.maxObjectListEntries),
      static_cast<unsigned>(options.objectTypeCount));

  BacnetObjectScanResult result;
  BacnetValue countValue;
  logger.debug("BACnet/Scan", "read device,%lu objectList[0] start",
               static_cast<unsigned long>(deviceInstance_));
  result.objectListCountStatus =
      readProperty(deviceObject(), BacnetPropertyId::ObjectList, countValue,
                   options.readTimeoutMs, 0);
  if (result.objectListCountStatus != BacnetDeviceSessionReadStatus::Ack) {
    logger.warn("BACnet/Scan", "read device,%lu objectList[0] failed: %s",
                static_cast<unsigned long>(deviceInstance_),
                bacnetReadStatusText(result.objectListCountStatus));
    logger.info(
        "BACnet/Scan",
        "scan summary count-status=%s count=%lu inspected=%lu found=%u stored=%u "
        "truncated=%s",
        bacnetReadStatusText(result.objectListCountStatus),
        static_cast<unsigned long>(result.objectListCount),
        static_cast<unsigned long>(result.inspected),
        static_cast<unsigned>(result.found),
        static_cast<unsigned>(result.stored),
        result.truncated ? "yes" : "no");
    return result;
  }
  if (countValue.type != BacnetValueType::Unsigned) {
    result.objectListCountStatus = BacnetDeviceSessionReadStatus::Error;
    logger.warn("BACnet/Scan",
                "read device,%lu objectList[0] invalid value type %u",
                static_cast<unsigned long>(deviceInstance_),
                static_cast<unsigned>(countValue.type));
    logger.info(
        "BACnet/Scan",
        "scan summary count-status=%s count=%lu inspected=%lu found=%u stored=%u "
        "truncated=%s",
        bacnetReadStatusText(result.objectListCountStatus),
        static_cast<unsigned long>(result.objectListCount),
        static_cast<unsigned long>(result.inspected),
        static_cast<unsigned>(result.found),
        static_cast<unsigned>(result.stored),
        result.truncated ? "yes" : "no");
    return result;
  }

  result.objectListCount = countValue.unsignedValue;
  logger.info("BACnet/Scan", "read device,%lu objectList[0]=%lu",
              static_cast<unsigned long>(deviceInstance_),
              static_cast<unsigned long>(result.objectListCount));
  const uint32_t maxEntries =
      result.objectListCount < options.maxObjectListEntries
          ? result.objectListCount
          : options.maxObjectListEntries;
  bool truncationLogged = false;

  for (uint32_t index = 1; index <= maxEntries; ++index) {
    BacnetValue entryValue;
    logger.trace("BACnet/Scan", "read device,%lu objectList[%lu] start",
                 static_cast<unsigned long>(deviceInstance_),
                 static_cast<unsigned long>(index));
    const BacnetDeviceSessionReadStatus entryStatus =
        readProperty(deviceObject(), BacnetPropertyId::ObjectList, entryValue,
                     options.readTimeoutMs, index);
    if (entryStatus != BacnetDeviceSessionReadStatus::Ack) {
      logger.warn("BACnet/Scan", "read device,%lu objectList[%lu] failed: %s",
                  static_cast<unsigned long>(deviceInstance_),
                  static_cast<unsigned long>(index),
                  bacnetReadStatusText(entryStatus));
      break;
    }

    ++result.inspected;
    BacnetObjectId objectId;
    if (!objectIdFromObjectListValue(entryValue, objectId)) {
      logger.trace("BACnet/Scan",
                   "objectList[%lu] skipped malformed object identifier",
                   static_cast<unsigned long>(index));
      continue;
    }

    if (!options.acceptsObjectType(objectId)) {
      logger.trace("BACnet/Scan", "objectList[%lu] skipped %s,%lu",
                   static_cast<unsigned long>(index),
                   bacnetObjectTypeText(objectId.type),
                   static_cast<unsigned long>(objectId.instance));
      continue;
    }

    logger.debug("BACnet/Scan", "objectList[%lu] accepted %s,%lu",
                 static_cast<unsigned long>(index),
                 bacnetObjectTypeText(objectId.type),
                 static_cast<unsigned long>(objectId.instance));

    ++result.found;
    if (results == nullptr) {
      result.truncated = true;
      if (!truncationLogged) {
        truncationLogged = true;
        logger.warn("BACnet/Scan",
                    "no result buffer provided; scan will truncate");
      }
      continue;
    }

    if (result.stored >= resultCapacity) {
      result.truncated = true;
      if (!truncationLogged) {
        truncationLogged = true;
        logger.warn("BACnet/Scan",
                    "result buffer full at %u entries; scan will truncate",
                    static_cast<unsigned>(resultCapacity));
      }
      continue;
    }

    BacnetScannedObject& scanned = results[result.stored++];
    scanned = BacnetScannedObject{};
    scanned.objectId = objectId;
    BacnetRemoteObject remoteObject = object(objectId);
    if (options.readObjectName) {
      scanned.objectNameStatus =
          remoteObject.readObjectName(scanned.objectName, options.readTimeoutMs);
    }
    if (options.readDescription) {
      scanned.descriptionStatus =
          remoteObject.readDescription(scanned.description,
                                       options.readTimeoutMs);
    }
    if (options.readPresentValue) {
      scanned.presentValueStatus =
          remoteObject.readPresentValue(scanned.presentValue,
                                        options.readTimeoutMs);
    }

    logger.debug(
        "BACnet/Scan",
        "scan read %s,%lu name=%s description=%s presentValue=%s",
        bacnetObjectTypeText(objectId.type),
        static_cast<unsigned long>(objectId.instance),
        bacnetReadStatusText(scanned.objectNameStatus),
        bacnetReadStatusText(scanned.descriptionStatus),
        bacnetReadStatusText(scanned.presentValueStatus));
  }

  if (result.objectListCount > options.maxObjectListEntries) {
    result.truncated = true;
  }

  logger.info(
      "BACnet/Scan",
      "scan summary count-status=%s count=%lu inspected=%lu found=%u stored=%u "
      "truncated=%s",
      bacnetReadStatusText(result.objectListCountStatus),
      static_cast<unsigned long>(result.objectListCount),
      static_cast<unsigned long>(result.inspected),
      static_cast<unsigned>(result.found),
      static_cast<unsigned>(result.stored),
      result.truncated ? "yes" : "no");
  return result;
}

void BacnetDeviceSession::poll(BacnetPropertySubscription& subscription,
                               uint32_t nowMs) {
  if (subscription.session_ != this) {
    return;
  }

  pollInFlightSubscription(nowMs);
  if (inFlightSubscription_ != nullptr) {
    return;
  }

  tryStartSubscriptionPoll(subscription, nowMs);
}

void BacnetDeviceSession::poll(BacnetPropertySubscription* subscriptions,
                               size_t count,
                               uint32_t nowMs) {
  pollInFlightSubscription(nowMs);
  if (inFlightSubscription_ != nullptr || subscriptions == nullptr || count == 0) {
    return;
  }

  if (roundRobinSubscriptionIndex_ >= count) {
    roundRobinSubscriptionIndex_ = 0;
  }

  for (size_t i = 0; i < count; ++i) {
    const size_t index = (roundRobinSubscriptionIndex_ + i) % count;
    BacnetPropertySubscription& subscription = subscriptions[index];
    if (subscription.session_ != this || !subscription.isDue(nowMs)) {
      continue;
    }

    roundRobinSubscriptionIndex_ = (index + 1) % count;
    tryStartSubscriptionPoll(subscription, nowMs);
    return;
  }
}

const char* BacnetDeviceSession::subscriptionPollTriggerText(
    BacnetPropertySubscription::PollTrigger trigger) {
  switch (trigger) {
    case BacnetPropertySubscription::PollTrigger::Initial:
      return "initial";
    case BacnetPropertySubscription::PollTrigger::Refresh:
      return "refresh";
    case BacnetPropertySubscription::PollTrigger::Fallback:
      return "fallback";
    case BacnetPropertySubscription::PollTrigger::None:
    default:
      return "unknown";
  }
}

bool BacnetDeviceSession::bacnetValueEquals(const BacnetValue& left,
                                            const BacnetValue& right) {
  if (left.type != right.type) {
    return false;
  }

  switch (left.type) {
    case BacnetValueType::Boolean:
      return left.booleanValue == right.booleanValue;
    case BacnetValueType::Unsigned:
    case BacnetValueType::Enumerated:
      return left.unsignedValue == right.unsignedValue;
    case BacnetValueType::Signed:
      return left.signedValue == right.signedValue;
    case BacnetValueType::Real:
      return left.realValue == right.realValue;
    case BacnetValueType::ObjectIdentifier:
      return left.objectValue.type == right.objectValue.type &&
             left.objectValue.instance == right.objectValue.instance;
    case BacnetValueType::CharacterString:
    case BacnetValueType::ObjectIdentifierList:
    case BacnetValueType::Error:
    case BacnetValueType::Unsupported:
      return textEquals(left.text, right.text);
    case BacnetValueType::Null:
    case BacnetValueType::Empty:
    default:
      return true;
  }
}

void BacnetDeviceSession::pollInFlightSubscription(uint32_t nowMs) {
  if (inFlightSubscription_ == nullptr) {
    return;
  }

  BacnetPropertySubscription& subscription = *inFlightSubscription_;
  if (!subscription.active_) {
    subscription.clearInFlightState();
    inFlightSubscription_ = nullptr;
    return;
  }

  const BacnetPropertyRequest request{subscription.objectId_,
                                      subscription.propertyId_,
                                      subscription.arrayIndex_};
  BacnetValue value;
  const BacnetReadPropertyPollStatus status =
      client_.pollReadPropertyStatus(value, subscription.inFlightInvokeId_,
                                     request);
  if (status == BacnetReadPropertyPollStatus::Ack) {
    finishSubscriptionPoll(subscription, BacnetDeviceSessionReadStatus::Ack,
                           &value, nowMs);
    return;
  }

  if (status == BacnetReadPropertyPollStatus::Error) {
    finishSubscriptionPoll(subscription, BacnetDeviceSessionReadStatus::Error,
                           &value, nowMs);
    return;
  }

  if (nowMs - subscription.inFlightStartedAt_ >= subscription.options_.timeoutMs) {
    client_.logReadPropertyTimeout(subscription.inFlightInvokeId_, request);
    finishSubscriptionPoll(subscription, BacnetDeviceSessionReadStatus::Timeout,
                           nullptr, nowMs);
  }
}

void BacnetDeviceSession::tryStartSubscriptionPoll(
    BacnetPropertySubscription& subscription,
    uint32_t nowMs) {
  if (!subscription.isDue(nowMs)) {
    return;
  }

  BacnetPropertySubscription::PollTrigger trigger =
      BacnetPropertySubscription::PollTrigger::Fallback;
  if (subscription.refreshRequested_) {
    trigger = BacnetPropertySubscription::PollTrigger::Refresh;
  } else if (subscription.initialReadPending_) {
    trigger = BacnetPropertySubscription::PollTrigger::Initial;
  }

  const BacnetPropertyRequest request{subscription.objectId_,
                                      subscription.propertyId_,
                                      subscription.arrayIndex_};
  const uint8_t invokeId = allocateInvokeId();
  client_.logger().debug(
      "BACnet/Subscription", "%s read start %s,%lu %u array=%lu",
      subscriptionPollTriggerText(trigger),
      bacnetObjectTypeText(subscription.objectId_.type),
      static_cast<unsigned long>(subscription.objectId_.instance),
      static_cast<unsigned>(subscription.propertyId_),
      static_cast<unsigned long>(subscription.arrayIndex_));

  if (!client_.sendReadProperty(address_, request, invokeId, port_)) {
    finishSubscriptionPoll(subscription,
                           BacnetDeviceSessionReadStatus::SendFailed,
                           nullptr, nowMs);
    return;
  }

  subscription.inFlight_ = true;
  subscription.inFlightInvokeId_ = invokeId;
  subscription.inFlightStartedAt_ = nowMs;
  subscription.inFlightTrigger_ = trigger;
  subscription.refreshRequested_ = false;
  inFlightSubscription_ = &subscription;
}

void BacnetDeviceSession::finishSubscriptionPoll(
    BacnetPropertySubscription& subscription,
    BacnetDeviceSessionReadStatus status,
    const BacnetValue* value,
    uint32_t nowMs) {
  const BacnetDeviceSessionReadStatus previousStatus = subscription.lastStatus_;
  const bool statusChanged = !subscription.hasTerminalStatus_ ||
                             previousStatus != status;

  bool firstValue = false;
  bool valueChanged = false;
  if (status == BacnetDeviceSessionReadStatus::Ack && value != nullptr) {
    firstValue = !subscription.hasValue_;
    valueChanged = firstValue ||
                   !bacnetValueEquals(subscription.lastValue_, *value);
    if (valueChanged) {
      subscription.lastValue_ = *value;
    }
    subscription.hasValue_ = true;
  }

  subscription.hasTerminalStatus_ = true;
  subscription.lastStatus_ = status;
  subscription.lastUpdateMs_ = nowMs;

  if (status == BacnetDeviceSessionReadStatus::Error ||
      status == BacnetDeviceSessionReadStatus::Timeout ||
      status == BacnetDeviceSessionReadStatus::SendFailed) {
    client_.logger().warn(
      "BACnet/Subscription", "terminal status %s for %s,%lu %u array=%lu",
      bacnetReadStatusText(status),
      bacnetObjectTypeText(subscription.objectId_.type),
      static_cast<unsigned long>(subscription.objectId_.instance),
      static_cast<unsigned>(subscription.propertyId_),
      static_cast<unsigned long>(subscription.arrayIndex_));
  }

  if (subscription.inFlightTrigger_ == BacnetPropertySubscription::PollTrigger::Initial) {
    client_.logger().debug(
      "BACnet/Subscription",
      "initial read completed status=%s %s,%lu %u array=%lu",
      bacnetReadStatusText(status),
      bacnetObjectTypeText(subscription.objectId_.type),
      static_cast<unsigned long>(subscription.objectId_.instance),
      static_cast<unsigned>(subscription.propertyId_),
      static_cast<unsigned long>(subscription.arrayIndex_));
  }

  BacnetSubscriptionNotificationReason reasons =
      BacnetSubscriptionNotificationReason::None;
  if (firstValue) {
    reasons = reasons | BacnetSubscriptionNotificationReason::FirstValue;
  }
  if (valueChanged) {
    reasons = reasons | BacnetSubscriptionNotificationReason::ValueChanged;
    client_.logger().debug(
      "BACnet/Subscription", "value changed %s,%lu %u array=%lu",
      bacnetObjectTypeText(subscription.objectId_.type),
      static_cast<unsigned long>(subscription.objectId_.instance),
      static_cast<unsigned>(subscription.propertyId_),
      static_cast<unsigned long>(subscription.arrayIndex_));
  }
  if (statusChanged && subscription.options_.notifyOnStatusChange) {
    reasons = reasons | BacnetSubscriptionNotificationReason::StatusChanged;
    client_.logger().debug(
        "BACnet/Subscription", "status changed %s->%s %s,%lu %u array=%lu",
        bacnetReadStatusText(previousStatus), bacnetReadStatusText(status),
        bacnetObjectTypeText(subscription.objectId_.type),
        static_cast<unsigned long>(subscription.objectId_.instance),
        static_cast<unsigned>(subscription.propertyId_),
        static_cast<unsigned long>(subscription.arrayIndex_));
  }

  subscription.lastNotificationReason_ = reasons;
  if (subscription.callback_ != nullptr &&
      reasons != BacnetSubscriptionNotificationReason::None) {
    const BacnetSubscriptionNotification notification{
        subscription.objectId_,
        subscription.propertyId_,
        subscription.arrayIndex_,
        status,
        subscription.hasValue_ ? &subscription.lastValue_ : nullptr,
        subscription.hasValue_,
        firstValue,
        valueChanged,
        statusChanged,
        reasons,
        subscription.userData_};
    subscription.callback_(notification);
  }

  subscription.initialReadPending_ = false;
  subscription.scheduleNextPoll(nowMs);
  subscription.clearInFlightState();
  if (inFlightSubscription_ == &subscription) {
    inFlightSubscription_ = nullptr;
  }
}

void BacnetDeviceSession::releaseSubscription(
    BacnetPropertySubscription& subscription) {
  if (inFlightSubscription_ == &subscription) {
    inFlightSubscription_ = nullptr;
  }
  subscription.clearInFlightState();
}

uint8_t BacnetDeviceSession::allocateInvokeId() {
  const uint8_t invokeId = nextInvokeId_;
  ++nextInvokeId_;
  if (nextInvokeId_ == 0) {
    nextInvokeId_ = 1;
  }
  return invokeId;
}

bool BacnetObjectScanOptions::acceptsObjectType(
    BacnetObjectId objectId) const {
  if (objectTypes == nullptr || objectTypeCount == 0) {
    return true;
  }
  for (size_t i = 0; i < objectTypeCount; ++i) {
    if (static_cast<uint16_t>(objectTypes[i]) == objectId.type) {
      return true;
    }
  }
  return false;
}

bool BacnetObjectScanOptions::acceptsObjectType(
    BacnetObjectType objectType) const {
  return acceptsObjectType(
      BacnetObjectId{static_cast<uint16_t>(objectType), 0});
}
