// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetDeviceSession.h"

#include "BacnetRemoteObject.h"

#include <cstring>

namespace {

constexpr uint32_t kBacnetErrorClassProperty = 2;
constexpr uint32_t kBacnetErrorCodeUnknownProperty = 32;
constexpr uint32_t kBacnetErrorCodeInvalidArrayIndex = 42;

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

bool propertyIdFromPropertyListValue(const BacnetValue& value,
                                     BacnetPropertyId& propertyId) {
  if (value.type != BacnetValueType::Enumerated &&
      value.type != BacnetValueType::Unsigned) {
    return false;
  }

  propertyId = static_cast<BacnetPropertyId>(value.unsignedValue);
  return true;
}

bool enumValueFromBacnetValue(const BacnetValue& value, uint32_t& output) {
  if (value.type != BacnetValueType::Enumerated &&
      value.type != BacnetValueType::Unsigned) {
    return false;
  }

  output = value.unsignedValue;
  return true;
}

bool booleanValueFromBacnetValue(const BacnetValue& value, bool& output) {
  if (value.type != BacnetValueType::Boolean) {
    return false;
  }

  output = value.booleanValue;
  return true;
}

BacnetPropertyReadStatus classifyDetailedReadStatus(
  BacnetReadPropertyPollStatus pollStatus,
  const BacnetValue& value,
  uint32_t errorClass,
  uint32_t errorCode,
  uint32_t requestArrayIndex) {
  switch (pollStatus) {
    case BacnetReadPropertyPollStatus::Ack:
      if (value.type == BacnetValueType::Unsupported) {
        return BacnetPropertyReadStatus::UnsupportedDatatype;
      }
      if (value.type == BacnetValueType::Empty ||
          value.type == BacnetValueType::Null) {
        return BacnetPropertyReadStatus::EmptyValue;
      }
      return BacnetPropertyReadStatus::Ack;
    case BacnetReadPropertyPollStatus::Error:
      if (errorClass == kBacnetErrorClassProperty &&
          errorCode == kBacnetErrorCodeUnknownProperty) {
        return BacnetPropertyReadStatus::UnsupportedProperty;
      }
      if (requestArrayIndex != kBacnetNoArrayIndex &&
          errorClass == kBacnetErrorClassProperty &&
          errorCode == kBacnetErrorCodeInvalidArrayIndex) {
        return BacnetPropertyReadStatus::ArrayIndexNotSupported;
      }
      return BacnetPropertyReadStatus::Error;
    case BacnetReadPropertyPollStatus::Reject:
      return BacnetPropertyReadStatus::Reject;
    case BacnetReadPropertyPollStatus::Abort:
      return BacnetPropertyReadStatus::Abort;
    case BacnetReadPropertyPollStatus::DecodeError:
      return BacnetPropertyReadStatus::DecodeError;
    case BacnetReadPropertyPollStatus::None:
      return BacnetPropertyReadStatus::Timeout;
  }

  return BacnetPropertyReadStatus::Error;
}

} // namespace

bool bacnetDecodeStatusFlags(const BacnetValue& value,
                             BacnetStatusFlags& flags) {
  if (value.type != BacnetValueType::BitString ||
      value.bitStringBitCount < 4) {
    return false;
  }

  flags.inAlarm = (value.bitStringValue & (1UL << 0)) != 0;
  flags.fault = (value.bitStringValue & (1UL << 1)) != 0;
  flags.overridden = (value.bitStringValue & (1UL << 2)) != 0;
  flags.outOfService = (value.bitStringValue & (1UL << 3)) != 0;
  return true;
}

const char* bacnetEventStateText(uint32_t eventState) {
  switch (eventState) {
    case 0:
      return "normal";
    case 1:
      return "fault";
    case 2:
      return "offnormal";
    case 3:
      return "high-limit";
    case 4:
      return "low-limit";
    case 5:
      return "life-safety-alarm";
    default:
      return "event-state";
  }
}

const char* bacnetReliabilityText(uint32_t reliability) {
  switch (reliability) {
    case 0:
      return "no-fault-detected";
    case 1:
      return "no-sensor";
    case 2:
      return "over-range";
    case 3:
      return "under-range";
    case 4:
      return "open-loop";
    case 5:
      return "shorted-loop";
    case 6:
      return "no-output";
    case 7:
      return "unreliable-other";
    case 8:
      return "process-error";
    case 9:
      return "multi-state-fault";
    case 10:
      return "configuration-error";
    case 12:
      return "communication-failure";
    case 13:
      return "member-fault";
    case 14:
      return "monitored-object-fault";
    case 15:
      return "tripped";
    default:
      return "reliability";
  }
}

BacnetObjectHealthState bacnetDeriveObjectHealthState(
  const BacnetObjectStatus& status,
  bool presentValueRequired) {
  if ((status.outOfServiceStatus == BacnetPropertyReadStatus::Ack &&
       status.outOfService) ||
      (status.statusFlagsStatus == BacnetPropertyReadStatus::Ack &&
       status.statusFlags.outOfService)) {
    return BacnetObjectHealthState::OutOfService;
  }

  if ((presentValueRequired &&
       status.presentValueStatus != BacnetPropertyReadStatus::Ack) ||
      (status.statusFlagsStatus == BacnetPropertyReadStatus::Ack &&
       status.statusFlags.fault) ||
      (status.reliabilityStatus == BacnetPropertyReadStatus::Ack &&
       status.reliability != 0) ||
      (status.eventStateStatus == BacnetPropertyReadStatus::Ack &&
       status.eventState == 1)) {
    return BacnetObjectHealthState::Error;
  }

  if ((status.statusFlagsStatus == BacnetPropertyReadStatus::Ack &&
       status.statusFlags.inAlarm) ||
      (status.eventStateStatus == BacnetPropertyReadStatus::Ack &&
       (status.eventState == 2 || status.eventState == 3 ||
        status.eventState == 4))) {
    return BacnetObjectHealthState::Warning;
  }

  if (status.statusFlagsStatus == BacnetPropertyReadStatus::Ack ||
      status.eventStateStatus == BacnetPropertyReadStatus::Ack ||
      status.reliabilityStatus == BacnetPropertyReadStatus::Ack ||
      status.outOfServiceStatus == BacnetPropertyReadStatus::Ack) {
    return BacnetObjectHealthState::Normal;
  }

  return BacnetObjectHealthState::Unknown;
}

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
      "BACnet/Subscription", "subscription stopped %s,%lu %u array=%lu", bacnetObjectTypeText(objectId_.type), static_cast<unsigned long>(objectId_.instance), static_cast<unsigned>(propertyId_), static_cast<unsigned long>(arrayIndex_));
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
                                         IPAddress address,
                                         uint16_t port)
    : client_(client),
      deviceInstance_(deviceInstance),
      address_(address),
      port_(port) {}

BacnetDeviceSession BacnetDeviceSession::fromEndpoint(BacnetClient& client,
                                                      uint32_t deviceInstance,
                                                      IPAddress address,
                                                      uint16_t port) {
  return BacnetDeviceSession(client, deviceInstance, address, port);
}

BacnetDeviceSession BacnetDeviceSession::fromIAm(BacnetClient& client,
                                                 const BacnetIAmDevice& device,
                                                 uint16_t port) {
  return fromEndpoint(client, device.deviceInstance, device.address, port);
}

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
  BacnetObjectId objectId, BacnetPropertyId property, BacnetValue& value, uint32_t timeoutMs, uint32_t arrayIndex) {
  const BacnetPropertyRequest request{objectId, property, arrayIndex};
  uint32_t errorClass = 0;
  uint32_t errorCode = 0;
  const BacnetPropertyReadStatus status =
    readPropertyDetailed(request, value, timeoutMs, errorClass, errorCode);

  switch (status) {
    case BacnetPropertyReadStatus::Ack:
    case BacnetPropertyReadStatus::EmptyValue:
    case BacnetPropertyReadStatus::UnsupportedDatatype:
      return BacnetDeviceSessionReadStatus::Ack;
    case BacnetPropertyReadStatus::Timeout:
      return BacnetDeviceSessionReadStatus::Timeout;
    case BacnetPropertyReadStatus::SendFailed:
      return BacnetDeviceSessionReadStatus::SendFailed;
    case BacnetPropertyReadStatus::Busy:
      return BacnetDeviceSessionReadStatus::Busy;
    default:
      return BacnetDeviceSessionReadStatus::Error;
  }
}

BacnetDeviceSessionReadStatus BacnetDeviceSession::readProperty(
  BacnetObjectType objectType, uint32_t objectInstance, BacnetPropertyId property, BacnetValue& value, uint32_t timeoutMs, uint32_t arrayIndex) {
  return readProperty(
    BacnetObjectId{static_cast<uint16_t>(objectType), objectInstance},
    property,
    value,
    timeoutMs,
    arrayIndex);
}

BacnetObjectHealthState BacnetDeviceSession::readObjectStatus(
  BacnetObjectId objectId,
  BacnetObjectStatus& status,
  uint32_t timeoutMs,
  bool presentValueRequired) {
  status = BacnetObjectStatus{};
  status.objectId = objectId;

  uint32_t errorClass = 0;
  uint32_t errorCode = 0;
  status.presentValueStatus = readPropertyDetailed(
    BacnetPropertyRequest{objectId, BacnetPropertyId::PresentValue, kBacnetNoArrayIndex},
    status.presentValue,
    timeoutMs,
    errorClass,
    errorCode);

  BacnetValue statusFlagsValue;
  status.statusFlagsStatus = readPropertyDetailed(
    BacnetPropertyRequest{objectId, BacnetPropertyId::StatusFlags, kBacnetNoArrayIndex},
    statusFlagsValue,
    timeoutMs,
    errorClass,
    errorCode);
  if (status.statusFlagsStatus == BacnetPropertyReadStatus::Ack &&
      !bacnetDecodeStatusFlags(statusFlagsValue, status.statusFlags)) {
    status.statusFlagsStatus = BacnetPropertyReadStatus::DecodeError;
  }

  BacnetValue eventStateValue;
  status.eventStateStatus = readPropertyDetailed(
    BacnetPropertyRequest{objectId, BacnetPropertyId::EventState, kBacnetNoArrayIndex},
    eventStateValue,
    timeoutMs,
    errorClass,
    errorCode);
  if (status.eventStateStatus == BacnetPropertyReadStatus::Ack &&
      !enumValueFromBacnetValue(eventStateValue, status.eventState)) {
    status.eventStateStatus = BacnetPropertyReadStatus::DecodeError;
  }

  BacnetValue reliabilityValue;
  status.reliabilityStatus = readPropertyDetailed(
    BacnetPropertyRequest{objectId, BacnetPropertyId::Reliability, kBacnetNoArrayIndex},
    reliabilityValue,
    timeoutMs,
    errorClass,
    errorCode);
  if (status.reliabilityStatus == BacnetPropertyReadStatus::Ack &&
      !enumValueFromBacnetValue(reliabilityValue, status.reliability)) {
    status.reliabilityStatus = BacnetPropertyReadStatus::DecodeError;
  }

  BacnetValue outOfServiceValue;
  status.outOfServiceStatus = readPropertyDetailed(
    BacnetPropertyRequest{objectId, BacnetPropertyId::OutOfService, kBacnetNoArrayIndex},
    outOfServiceValue,
    timeoutMs,
    errorClass,
    errorCode);
  if (status.outOfServiceStatus == BacnetPropertyReadStatus::Ack &&
      !booleanValueFromBacnetValue(outOfServiceValue, status.outOfService)) {
    status.outOfServiceStatus = BacnetPropertyReadStatus::DecodeError;
  }

  status.state = bacnetDeriveObjectHealthState(status, presentValueRequired);
  return status.state;
}

BacnetObjectHealthState BacnetDeviceSession::readObjectStatus(
  BacnetObjectType objectType,
  uint32_t objectInstance,
  BacnetObjectStatus& status,
  uint32_t timeoutMs,
  bool presentValueRequired) {
  return readObjectStatus(
    BacnetObjectId{static_cast<uint16_t>(objectType), objectInstance},
    status,
    timeoutMs,
    presentValueRequired);
}

BacnetPropertyReadStatus BacnetDeviceSession::readPropertyDetailed(
  const BacnetPropertyRequest& request,
  BacnetValue& value,
  uint32_t timeoutMs,
  uint32_t& errorClass,
  uint32_t& errorCode) {
  value = BacnetValue{};
  errorClass = 0;
  errorCode = 0;

  if (inFlightSubscription_ != nullptr || inFlightObjectListScan_ != nullptr) {
    client_.logger().warn(
      "BACnet/ReadProperty",
      "ReadProperty %s,%lu property=%lu skipped: session busy",
      bacnetObjectTypeText(request.object.type),
      static_cast<unsigned long>(request.object.instance),
      static_cast<unsigned long>(request.property));
    return BacnetPropertyReadStatus::Busy;
  }

  const uint8_t invokeId = allocateInvokeId();
  if (!client_.sendReadProperty(address_, request, invokeId, port_)) {
    return BacnetPropertyReadStatus::SendFailed;
  }

  const unsigned long startedAt = millis();
  while (true) {
    const BacnetReadPropertyPollStatus pollStatus =
      client_.pollReadPropertyStatus(value, invokeId, request, &errorClass, &errorCode);
    if (pollStatus != BacnetReadPropertyPollStatus::None) {
      return classifyDetailedReadStatus(pollStatus, value, errorClass, errorCode, request.arrayIndex);
    }
    if (millis() - startedAt >= timeoutMs) {
      client_.logReadPropertyTimeout(invokeId, request);
      return BacnetPropertyReadStatus::Timeout;
    }
    yield();
  }
}

BacnetPropertyListReadResult BacnetDeviceSession::readPropertyList(
  BacnetObjectId object,
  BacnetPropertyId* properties,
  size_t propertyCapacity,
  uint32_t timeoutMs) {
  BacnetPropertyListReadResult result;
  BacnetValue countValue;
  uint32_t errorClass = 0;
  uint32_t errorCode = 0;

  result.status = readPropertyDetailed(
    BacnetPropertyRequest{object, BacnetPropertyId::PropertyList, 0},
    countValue,
    timeoutMs,
    errorClass,
    errorCode);
  if (result.status != BacnetPropertyReadStatus::Ack) {
    return result;
  }
  if (countValue.type != BacnetValueType::Unsigned) {
    result.status = BacnetPropertyReadStatus::DecodeError;
    return result;
  }

  result.advertised = countValue.unsignedValue;
  result.truncated = result.advertised > propertyCapacity;
  const size_t limit = result.advertised < propertyCapacity
                         ? static_cast<size_t>(result.advertised)
                         : propertyCapacity;

  for (size_t i = 0; i < limit; ++i) {
    BacnetValue propertyValue;
    const BacnetPropertyReadStatus status = readPropertyDetailed(
      BacnetPropertyRequest{object, BacnetPropertyId::PropertyList, static_cast<uint32_t>(i + 1)},
      propertyValue,
      timeoutMs,
      errorClass,
      errorCode);
    if (status != BacnetPropertyReadStatus::Ack) {
      result.status = status;
      return result;
    }

    BacnetPropertyId propertyId = BacnetPropertyId::ObjectName;
    if (!propertyIdFromPropertyListValue(propertyValue, propertyId)) {
      result.status = BacnetPropertyReadStatus::DecodeError;
      return result;
    }

    if (properties != nullptr) {
      properties[result.stored] = propertyId;
    }
    ++result.stored;
  }

  return result;
}

BacnetPropertyListReadResult BacnetDeviceSession::readPropertyList(
  BacnetObjectType objectType,
  uint32_t objectInstance,
  BacnetPropertyId* properties,
  size_t propertyCapacity,
  uint32_t timeoutMs) {
  return readPropertyList(
    BacnetObjectId{static_cast<uint16_t>(objectType), objectInstance},
    properties,
    propertyCapacity,
    timeoutMs);
}

BacnetPropertyReadAllResult BacnetDeviceSession::readAllProperties(
  BacnetObjectId object,
  const BacnetPropertyId* properties,
  size_t propertyCount,
  BacnetPropertyReadResult* results,
  size_t resultCapacity,
  uint32_t timeoutMs) {
  BacnetPropertyReadAllResult summary;
  summary.requested = propertyCount;
  if (results == nullptr) {
    resultCapacity = 0;
  }
  summary.truncated = propertyCount > resultCapacity;

  const size_t limit = propertyCount < resultCapacity ? propertyCount : resultCapacity;
  for (size_t i = 0; i < limit; ++i) {
    BacnetPropertyReadResult& entry = results[i];
    entry = BacnetPropertyReadResult{};
    entry.propertyId = properties[i];

    uint32_t errorClass = 0;
    uint32_t errorCode = 0;
    entry.status = readPropertyDetailed(
      BacnetPropertyRequest{object, properties[i], kBacnetNoArrayIndex},
      entry.value,
      timeoutMs,
      errorClass,
      errorCode);

    ++summary.attempted;
    ++summary.stored;
    if (entry.status == BacnetPropertyReadStatus::Ack) {
      ++summary.acked;
    } else {
      ++summary.failed;
    }
  }

  return summary;
}

BacnetPropertyReadAllResult BacnetDeviceSession::readAllProperties(
  BacnetObjectType objectType,
  uint32_t objectInstance,
  const BacnetPropertyId* properties,
  size_t propertyCount,
  BacnetPropertyReadResult* results,
  size_t resultCapacity,
  uint32_t timeoutMs) {
  return readAllProperties(
    BacnetObjectId{static_cast<uint16_t>(objectType), objectInstance},
    properties,
    propertyCount,
    results,
    resultCapacity,
    timeoutMs);
}

BacnetObjectScanResult BacnetDeviceSession::scanObjectList(
  const BacnetObjectScanOptions& options,
  BacnetScannedObject* results,
  size_t resultCapacity) {
  BacnetObjectListScanJob job;
  if (!beginObjectListScan(job, options, results, resultCapacity)) {
    return job.summary();
  }

  while (job.isActive()) {
    pollObjectListScan(job);
    yield();
  }
  return job.summary();
}

bool BacnetDeviceSession::beginObjectListScan(
  BacnetObjectListScanJob& job,
  const BacnetObjectScanOptions& options,
  BacnetScannedObject* results,
  size_t resultCapacity,
  uint32_t nowMs) {
  BacnetLogger& logger = client_.logger();
  if (inFlightObjectListScan_ != nullptr || inFlightSubscription_ != nullptr ||
      job.isActive()) {
    logger.warn("BACnet/Scan", "scan start skipped: session busy");
    return false;
  }

  job.clear();
  job.session_ = this;
  job.options_ = options;
  job.results_ = results;
  job.resultCapacity_ = resultCapacity;
  job.status_ = BacnetObjectListScanJobStatus::Active;
  job.phase_ = BacnetObjectListScanPhase::ReadObjectListCount;
  inFlightObjectListScan_ = &job;

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
  logger.debug("BACnet/Scan", "read device,%lu objectList[0] start", static_cast<unsigned long>(deviceInstance_));

  const BacnetPropertyRequest countRequest{
    deviceObject(), BacnetPropertyId::ObjectList, 0};
  if (!tryStartObjectListScanRead(
        job, countRequest, BacnetObjectListScanPhase::ReadObjectListCount, nowMs)) {
    return false;
  }
  return true;
}

void BacnetDeviceSession::pollObjectListScan(BacnetObjectListScanJob& job,
                                             uint32_t nowMs) {
  if (job.session_ != this || !job.isActive()) {
    return;
  }
  pollInFlightObjectListScan(nowMs);
  if (job.isActive() && !job.requestInFlight_) {
    advanceObjectListScan(job, nowMs);
  }
}

void BacnetDeviceSession::cancelObjectListScan(BacnetObjectListScanJob& job) {
  if (job.session_ != this || job.isTerminal()) {
    return;
  }
  client_.logger().warn("BACnet/Scan", "scan cancelled device %lu", static_cast<unsigned long>(deviceInstance_));
  releaseObjectListScan(job);
  job.status_ = BacnetObjectListScanJobStatus::Cancelled;
  job.phase_ = BacnetObjectListScanPhase::Cancelled;
}

bool BacnetDeviceSession::tryStartObjectListScanRead(
  BacnetObjectListScanJob& job,
  const BacnetPropertyRequest& request,
  BacnetObjectListScanPhase phase,
  uint32_t nowMs) {
  const uint8_t invokeId = allocateInvokeId();
  job.phase_ = phase;
  job.inFlightRequest_ = request;
  job.inFlightValue_ = BacnetValue{};
  if (!client_.sendReadProperty(address_, request, invokeId, port_)) {
    finishObjectListScanRead(job, BacnetDeviceSessionReadStatus::SendFailed, nullptr, nowMs);
    return false;
  }

  job.inFlightInvokeId_ = invokeId;
  job.inFlightStartedAtMs_ = nowMs;
  job.requestInFlight_ = true;
  return true;
}

void BacnetDeviceSession::pollInFlightObjectListScan(uint32_t nowMs) {
  if (inFlightObjectListScan_ == nullptr) {
    return;
  }

  BacnetObjectListScanJob& job = *inFlightObjectListScan_;
  if (!job.isActive() || !job.requestInFlight_) {
    return;
  }

  BacnetValue value;
  const BacnetReadPropertyPollStatus status = client_.pollReadPropertyStatus(
    value, job.inFlightInvokeId_, job.inFlightRequest_);
  if (status == BacnetReadPropertyPollStatus::Ack) {
    finishObjectListScanRead(job, BacnetDeviceSessionReadStatus::Ack, &value, nowMs);
    return;
  }
  if (status == BacnetReadPropertyPollStatus::Error) {
    finishObjectListScanRead(job, BacnetDeviceSessionReadStatus::Error, &value, nowMs);
    return;
  }
  if (nowMs - job.inFlightStartedAtMs_ >= job.options_.readTimeoutMs) {
    client_.logReadPropertyTimeout(job.inFlightInvokeId_, job.inFlightRequest_);
    finishObjectListScanRead(job, BacnetDeviceSessionReadStatus::Timeout, nullptr, nowMs);
  }
}

void BacnetDeviceSession::finishObjectListScanRead(
  BacnetObjectListScanJob& job,
  BacnetDeviceSessionReadStatus status,
  const BacnetValue* value,
  uint32_t nowMs) {
  const BacnetObjectListScanPhase phase = job.phase_;
  job.clearInFlightState();

  if (phase == BacnetObjectListScanPhase::ReadObjectListCount) {
    job.result_.objectListCountStatus = status;
    if (status != BacnetDeviceSessionReadStatus::Ack) {
      failObjectListScan(job, status);
      return;
    }
    if (value == nullptr || value->type != BacnetValueType::Unsigned) {
      client_.logger().warn("BACnet/Scan",
                            "read device,%lu objectList[0] invalid value type %u",
                            static_cast<unsigned long>(deviceInstance_),
                            value != nullptr ? static_cast<unsigned>(value->type)
                                             : 0);
      failObjectListScan(job, BacnetDeviceSessionReadStatus::Error);
      return;
    }
    job.result_.objectListCount = value->unsignedValue;
    job.maxIndex_ = job.result_.objectListCount < job.options_.maxObjectListEntries
                      ? job.result_.objectListCount
                      : job.options_.maxObjectListEntries;
    if (job.result_.objectListCount > job.options_.maxObjectListEntries) {
      job.result_.truncated = true;
    }
    job.currentIndex_ = 1;
    client_.logger().info("BACnet/Scan", "read device,%lu objectList[0]=%lu", static_cast<unsigned long>(deviceInstance_), static_cast<unsigned long>(job.result_.objectListCount));
    advanceObjectListScan(job, nowMs);
    return;
  }

  if (phase == BacnetObjectListScanPhase::ReadObjectListEntry) {
    if (status != BacnetDeviceSessionReadStatus::Ack) {
      client_.logger().warn("BACnet/Scan",
                            "read device,%lu objectList[%lu] failed: %s",
                            static_cast<unsigned long>(deviceInstance_),
                            static_cast<unsigned long>(job.currentIndex_),
                            bacnetReadStatusText(status));
      completeObjectListScan(job);
      return;
    }

    ++job.result_.inspected;
    BacnetObjectId objectId;
    if (value == nullptr || !objectIdFromObjectListValue(*value, objectId)) {
      client_.logger().trace(
        "BACnet/Scan", "objectList[%lu] skipped malformed object identifier", static_cast<unsigned long>(job.currentIndex_));
      ++job.currentIndex_;
      advanceObjectListScan(job, nowMs);
      return;
    }

    if (!job.options_.acceptsObjectType(objectId)) {
      client_.logger().trace("BACnet/Scan", "objectList[%lu] skipped %s,%lu", static_cast<unsigned long>(job.currentIndex_), bacnetObjectTypeText(objectId.type), static_cast<unsigned long>(objectId.instance));
      ++job.currentIndex_;
      advanceObjectListScan(job, nowMs);
      return;
    }

    client_.logger().debug("BACnet/Scan", "objectList[%lu] accepted %s,%lu", static_cast<unsigned long>(job.currentIndex_), bacnetObjectTypeText(objectId.type), static_cast<unsigned long>(objectId.instance));

    ++job.result_.found;
    job.currentObject_ = objectId;
    job.hasCurrentStoreIndex_ = false;
    if (job.results_ == nullptr) {
      job.result_.truncated = true;
      if (!job.truncationLogged_) {
        job.truncationLogged_ = true;
        client_.logger().warn("BACnet/Scan",
                              "no result buffer provided; scan will truncate");
      }
      ++job.currentIndex_;
      advanceObjectListScan(job, nowMs);
      return;
    }
    if (job.result_.stored >= job.resultCapacity_) {
      job.result_.truncated = true;
      if (!job.truncationLogged_) {
        job.truncationLogged_ = true;
        client_.logger().warn(
          "BACnet/Scan", "result buffer full at %u entries; scan will truncate", static_cast<unsigned>(job.resultCapacity_));
      }
      ++job.currentIndex_;
      advanceObjectListScan(job, nowMs);
      return;
    }

    job.currentStoreIndex_ = job.result_.stored++;
    job.hasCurrentStoreIndex_ = true;
    BacnetScannedObject& scanned = job.results_[job.currentStoreIndex_];
    scanned = BacnetScannedObject{};
    scanned.objectId = objectId;
    advanceObjectListScan(job, nowMs);
    return;
  }

  if (!job.hasCurrentStoreIndex_ || job.results_ == nullptr) {
    ++job.currentIndex_;
    advanceObjectListScan(job, nowMs);
    return;
  }

  BacnetScannedObject& scanned = job.results_[job.currentStoreIndex_];
  if (phase == BacnetObjectListScanPhase::ReadObjectName) {
    scanned.objectNameStatus = status;
    if (status == BacnetDeviceSessionReadStatus::Ack && value != nullptr) {
      scanned.objectName = *value;
    }
  } else if (phase == BacnetObjectListScanPhase::ReadDescription) {
    scanned.descriptionStatus = status;
    if (status == BacnetDeviceSessionReadStatus::Ack && value != nullptr) {
      scanned.description = *value;
    }
  } else if (phase == BacnetObjectListScanPhase::ReadPresentValue) {
    scanned.presentValueStatus = status;
    if (status == BacnetDeviceSessionReadStatus::Ack && value != nullptr) {
      scanned.presentValue = *value;
    }
    client_.logger().debug(
      "BACnet/Scan",
      "scan read %s,%lu name=%s description=%s presentValue=%s",
      bacnetObjectTypeText(scanned.objectId.type),
      static_cast<unsigned long>(scanned.objectId.instance),
      bacnetReadStatusText(scanned.objectNameStatus),
      bacnetReadStatusText(scanned.descriptionStatus),
      bacnetReadStatusText(scanned.presentValueStatus));
  }

  advanceObjectListScan(job, nowMs);
}

void BacnetDeviceSession::advanceObjectListScan(BacnetObjectListScanJob& job,
                                                uint32_t nowMs) {
  if (!job.isActive() || job.requestInFlight_) {
    return;
  }

  if (job.hasCurrentStoreIndex_ && job.results_ != nullptr) {
    BacnetScannedObject& scanned = job.results_[job.currentStoreIndex_];
    if (job.options_.readObjectName &&
        scanned.objectNameStatus == BacnetDeviceSessionReadStatus::Skipped) {
      const BacnetPropertyRequest request{
        scanned.objectId, BacnetPropertyId::ObjectName, kBacnetNoArrayIndex};
      tryStartObjectListScanRead(job, request, BacnetObjectListScanPhase::ReadObjectName, nowMs);
      return;
    }
    if (job.options_.readDescription &&
        scanned.descriptionStatus == BacnetDeviceSessionReadStatus::Skipped) {
      const BacnetPropertyRequest request{
        scanned.objectId, BacnetPropertyId::Description, kBacnetNoArrayIndex};
      tryStartObjectListScanRead(job, request, BacnetObjectListScanPhase::ReadDescription, nowMs);
      return;
    }
    if (job.options_.readPresentValue &&
        scanned.presentValueStatus == BacnetDeviceSessionReadStatus::Skipped) {
      const BacnetPropertyRequest request{
        scanned.objectId, BacnetPropertyId::PresentValue, kBacnetNoArrayIndex};
      tryStartObjectListScanRead(job, request, BacnetObjectListScanPhase::ReadPresentValue, nowMs);
      return;
    }
    job.hasCurrentStoreIndex_ = false;
    ++job.currentIndex_;
  }

  if (job.currentIndex_ == 0 || job.currentIndex_ > job.maxIndex_) {
    completeObjectListScan(job);
    return;
  }

  client_.logger().trace("BACnet/Scan", "read device,%lu objectList[%lu] start", static_cast<unsigned long>(deviceInstance_), static_cast<unsigned long>(job.currentIndex_));
  const BacnetPropertyRequest request{
    deviceObject(), BacnetPropertyId::ObjectList, job.currentIndex_};
  tryStartObjectListScanRead(job, request, BacnetObjectListScanPhase::ReadObjectListEntry, nowMs);
}

void BacnetDeviceSession::completeObjectListScan(BacnetObjectListScanJob& job) {
  releaseObjectListScan(job);
  job.status_ = BacnetObjectListScanJobStatus::Complete;
  job.phase_ = BacnetObjectListScanPhase::Complete;
  client_.logger().info(
    "BACnet/Scan",
    "scan summary count-status=%s count=%lu inspected=%lu found=%u stored=%u "
    "truncated=%s",
    bacnetReadStatusText(job.result_.objectListCountStatus),
    static_cast<unsigned long>(job.result_.objectListCount),
    static_cast<unsigned long>(job.result_.inspected),
    static_cast<unsigned>(job.result_.found),
    static_cast<unsigned>(job.result_.stored),
    job.result_.truncated ? "yes" : "no");
}

void BacnetDeviceSession::failObjectListScan(
  BacnetObjectListScanJob& job,
  BacnetDeviceSessionReadStatus status) {
  if (job.result_.objectListCountStatus == BacnetDeviceSessionReadStatus::Skipped) {
    job.result_.objectListCountStatus = status;
  }
  client_.logger().warn("BACnet/Scan", "read device,%lu objectList[0] failed: %s", static_cast<unsigned long>(deviceInstance_), bacnetReadStatusText(status));
  releaseObjectListScan(job);
  job.status_ = BacnetObjectListScanJobStatus::Failed;
  job.phase_ = BacnetObjectListScanPhase::Failed;
  client_.logger().info(
    "BACnet/Scan",
    "scan summary count-status=%s count=%lu inspected=%lu found=%u stored=%u "
    "truncated=%s",
    bacnetReadStatusText(job.result_.objectListCountStatus),
    static_cast<unsigned long>(job.result_.objectListCount),
    static_cast<unsigned long>(job.result_.inspected),
    static_cast<unsigned>(job.result_.found),
    static_cast<unsigned>(job.result_.stored),
    job.result_.truncated ? "yes" : "no");
}

void BacnetDeviceSession::releaseObjectListScan(BacnetObjectListScanJob& job) {
  if (inFlightObjectListScan_ == &job) {
    inFlightObjectListScan_ = nullptr;
  }
  job.clearInFlightState();
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
    case BacnetValueType::BitString:
      return left.bitStringValue == right.bitStringValue &&
             left.bitStringBitCount == right.bitStringBitCount;
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
    client_.pollReadPropertyStatus(value, subscription.inFlightInvokeId_, request);
  if (status == BacnetReadPropertyPollStatus::Ack) {
    finishSubscriptionPoll(subscription, BacnetDeviceSessionReadStatus::Ack, &value, nowMs);
    return;
  }

  if (status == BacnetReadPropertyPollStatus::Error) {
    finishSubscriptionPoll(subscription, BacnetDeviceSessionReadStatus::Error, &value, nowMs);
    return;
  }

  if (nowMs - subscription.inFlightStartedAt_ >= subscription.options_.timeoutMs) {
    client_.logReadPropertyTimeout(subscription.inFlightInvokeId_, request);
    finishSubscriptionPoll(subscription, BacnetDeviceSessionReadStatus::Timeout, nullptr, nowMs);
  }
}

void BacnetDeviceSession::tryStartSubscriptionPoll(
  BacnetPropertySubscription& subscription,
  uint32_t nowMs) {
  if (!subscription.isDue(nowMs) || inFlightObjectListScan_ != nullptr) {
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
    "BACnet/Subscription", "%s read start %s,%lu %u array=%lu", subscriptionPollTriggerText(trigger), bacnetObjectTypeText(subscription.objectId_.type), static_cast<unsigned long>(subscription.objectId_.instance), static_cast<unsigned>(subscription.propertyId_), static_cast<unsigned long>(subscription.arrayIndex_));

  if (!client_.sendReadProperty(address_, request, invokeId, port_)) {
    finishSubscriptionPoll(subscription,
                           BacnetDeviceSessionReadStatus::SendFailed,
                           nullptr,
                           nowMs);
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
      "BACnet/Subscription", "terminal status %s for %s,%lu %u array=%lu", bacnetReadStatusText(status), bacnetObjectTypeText(subscription.objectId_.type), static_cast<unsigned long>(subscription.objectId_.instance), static_cast<unsigned>(subscription.propertyId_), static_cast<unsigned long>(subscription.arrayIndex_));
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

  uint8_t reasonBits = 0;
  if (firstValue) {
    reasonBits |= static_cast<uint8_t>(BacnetSubscriptionNotificationReason::FirstValue);
  }
  if (valueChanged) {
    reasonBits |= static_cast<uint8_t>(BacnetSubscriptionNotificationReason::ValueChanged);
    client_.logger().debug(
      "BACnet/Subscription",
      "value changed %s,%lu %u array=%lu",
      bacnetObjectTypeText(subscription.objectId_.type),
      static_cast<unsigned long>(subscription.objectId_.instance),
      static_cast<unsigned>(subscription.propertyId_),
      static_cast<unsigned long>(subscription.arrayIndex_));
  }
  if (statusChanged && subscription.options_.notifyOnStatusChange) {
    reasonBits |= static_cast<uint8_t>(BacnetSubscriptionNotificationReason::StatusChanged);
    client_.logger().debug(
      "BACnet/Subscription",
      "status changed %s->%s %s,%lu %u array=%lu",
      bacnetReadStatusText(previousStatus),
      bacnetReadStatusText(status),
      bacnetObjectTypeText(subscription.objectId_.type),
      static_cast<unsigned long>(subscription.objectId_.instance),
      static_cast<unsigned>(subscription.propertyId_),
      static_cast<unsigned long>(subscription.arrayIndex_));
  }

  const BacnetSubscriptionNotificationReason reasons =
    static_cast<BacnetSubscriptionNotificationReason>(reasonBits);
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
  BacnetObjectId objectTypeId) const {
  if (objectTypes == nullptr || objectTypeCount == 0) {
    return true;
  }
  for (size_t i = 0; i < objectTypeCount; ++i) {
    if (static_cast<uint16_t>(objectTypes[i]) == objectTypeId.type) {
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

BacnetObjectListScanJobStatus BacnetObjectListScanJob::status() const {
  return status_;
}

BacnetObjectListScanPhase BacnetObjectListScanJob::phase() const {
  return phase_;
}

bool BacnetObjectListScanJob::isIdle() const {
  return status_ == BacnetObjectListScanJobStatus::Idle;
}

bool BacnetObjectListScanJob::isActive() const {
  return status_ == BacnetObjectListScanJobStatus::Active;
}

bool BacnetObjectListScanJob::isComplete() const {
  return status_ == BacnetObjectListScanJobStatus::Complete;
}

bool BacnetObjectListScanJob::isFailed() const {
  return status_ == BacnetObjectListScanJobStatus::Failed;
}

bool BacnetObjectListScanJob::isCancelled() const {
  return status_ == BacnetObjectListScanJobStatus::Cancelled;
}

bool BacnetObjectListScanJob::isTerminal() const {
  return isComplete() || isFailed() || isCancelled();
}

bool BacnetObjectListScanJob::requestInFlight() const {
  return requestInFlight_;
}

uint32_t BacnetObjectListScanJob::currentIndex() const {
  return currentIndex_;
}

const BacnetObjectScanResult& BacnetObjectListScanJob::summary() const {
  return result_;
}

BacnetObjectListScanProgress BacnetObjectListScanJob::progress() const {
  return BacnetObjectListScanProgress{
    status_, phase_, currentIndex_, requestInFlight_, result_};
}

void BacnetObjectListScanJob::clear() {
  session_ = nullptr;
  options_ = BacnetObjectScanOptions{};
  results_ = nullptr;
  resultCapacity_ = 0;
  result_ = BacnetObjectScanResult{};
  status_ = BacnetObjectListScanJobStatus::Idle;
  phase_ = BacnetObjectListScanPhase::Idle;
  currentIndex_ = 0;
  maxIndex_ = 0;
  clearInFlightState();
  inFlightRequest_ = BacnetPropertyRequest{};
  inFlightValue_ = BacnetValue{};
  currentObject_ = BacnetObjectId{};
  currentStoreIndex_ = 0;
  hasCurrentStoreIndex_ = false;
  truncationLogged_ = false;
}

void BacnetObjectListScanJob::clearInFlightState() {
  requestInFlight_ = false;
  inFlightInvokeId_ = 0;
  inFlightStartedAtMs_ = 0;
}
