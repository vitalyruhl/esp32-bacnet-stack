// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "BacnetClient.h"

#include <cstddef>
#include <cstdint>

class BacnetDeviceSession;
class BacnetObjectListScanJob;
class BacnetProperty;
class BacnetRemoteObject;
class BacnetPropertySubscription;
struct BacnetObjectScanOptions;
struct BacnetObjectScanResult;
struct BacnetScannedObject;

enum class BacnetObjectListScanJobStatus : uint8_t {
  Idle,
  Active,
  Complete,
  Failed,
  Cancelled,
};

inline const char* bacnetObjectListScanJobStatusText(
    BacnetObjectListScanJobStatus status) {
  switch (status) {
    case BacnetObjectListScanJobStatus::Idle:
      return "idle";
    case BacnetObjectListScanJobStatus::Active:
      return "active";
    case BacnetObjectListScanJobStatus::Complete:
      return "complete";
    case BacnetObjectListScanJobStatus::Failed:
      return "failed";
    case BacnetObjectListScanJobStatus::Cancelled:
      return "cancelled";
  }
  return "unknown";
}

enum class BacnetObjectListScanPhase : uint8_t {
  Idle,
  ReadObjectListCount,
  ReadObjectListEntry,
  ReadObjectName,
  ReadDescription,
  ReadPresentValue,
  Complete,
  Failed,
  Cancelled,
};

inline const char* bacnetObjectListScanPhaseText(
    BacnetObjectListScanPhase phase) {
  switch (phase) {
    case BacnetObjectListScanPhase::Idle:
      return "idle";
    case BacnetObjectListScanPhase::ReadObjectListCount:
      return "read-object-list-count";
    case BacnetObjectListScanPhase::ReadObjectListEntry:
      return "read-object-list-entry";
    case BacnetObjectListScanPhase::ReadObjectName:
      return "read-object-name";
    case BacnetObjectListScanPhase::ReadDescription:
      return "read-description";
    case BacnetObjectListScanPhase::ReadPresentValue:
      return "read-present-value";
    case BacnetObjectListScanPhase::Complete:
      return "complete";
    case BacnetObjectListScanPhase::Failed:
      return "failed";
    case BacnetObjectListScanPhase::Cancelled:
      return "cancelled";
  }
  return "unknown";
}

enum class BacnetDeviceSessionReadStatus : uint8_t {
  Ack,
  Error,
  Timeout,
  SendFailed,
  Skipped,
  Busy,
};

inline const char* bacnetReadStatusText(BacnetDeviceSessionReadStatus status) {
  switch (status) {
    case BacnetDeviceSessionReadStatus::Ack:
      return "ok";
    case BacnetDeviceSessionReadStatus::Error:
      return "error";
    case BacnetDeviceSessionReadStatus::Timeout:
      return "timeout";
    case BacnetDeviceSessionReadStatus::SendFailed:
      return "send-failed";
    case BacnetDeviceSessionReadStatus::Skipped:
      return "skipped";
    case BacnetDeviceSessionReadStatus::Busy:
      return "busy";
  }
  return "unknown";
}

enum class BacnetPropertyReadStatus : uint8_t {
  Ack,
  UnsupportedProperty,
  Timeout,
  Error,
  Reject,
  Abort,
  DecodeError,
  EmptyValue,
  UnsupportedDatatype,
  ArrayIndexNotSupported,
  SendFailed,
  Busy,
  Skipped,
};

inline const char* bacnetPropertyReadStatusText(BacnetPropertyReadStatus status) {
  switch (status) {
    case BacnetPropertyReadStatus::Ack:
      return "ok";
    case BacnetPropertyReadStatus::UnsupportedProperty:
      return "unsupported-property";
    case BacnetPropertyReadStatus::Timeout:
      return "timeout";
    case BacnetPropertyReadStatus::Error:
      return "error";
    case BacnetPropertyReadStatus::Reject:
      return "reject";
    case BacnetPropertyReadStatus::Abort:
      return "abort";
    case BacnetPropertyReadStatus::DecodeError:
      return "decode-error";
    case BacnetPropertyReadStatus::EmptyValue:
      return "empty";
    case BacnetPropertyReadStatus::UnsupportedDatatype:
      return "unsupported-datatype";
    case BacnetPropertyReadStatus::ArrayIndexNotSupported:
      return "array-index-not-supported";
    case BacnetPropertyReadStatus::SendFailed:
      return "send-failed";
    case BacnetPropertyReadStatus::Busy:
      return "busy";
    case BacnetPropertyReadStatus::Skipped:
      return "skipped";
  }
  return "unknown";
}

enum class BacnetObjectHealthState : uint8_t {
  Unknown,
  Normal,
  Warning,
  Error,
  OutOfService,
};

inline const char* bacnetObjectHealthStateText(
    BacnetObjectHealthState state) {
  switch (state) {
    case BacnetObjectHealthState::Normal:
      return "Normal";
    case BacnetObjectHealthState::Warning:
      return "Warning";
    case BacnetObjectHealthState::Error:
      return "Error";
    case BacnetObjectHealthState::OutOfService:
      return "OutOfService";
    case BacnetObjectHealthState::Unknown:
    default:
      return "Unknown";
  }
}

struct BacnetStatusFlags {
  bool inAlarm = false;
  bool fault = false;
  bool overridden = false;
  bool outOfService = false;
};

struct BacnetObjectStatus {
  BacnetObjectId objectId;
  BacnetPropertyReadStatus presentValueStatus =
      BacnetPropertyReadStatus::Skipped;
  BacnetValue presentValue;
  BacnetPropertyReadStatus statusFlagsStatus =
      BacnetPropertyReadStatus::Skipped;
  BacnetStatusFlags statusFlags;
  BacnetPropertyReadStatus eventStateStatus =
      BacnetPropertyReadStatus::Skipped;
  uint32_t eventState = 0;
  BacnetPropertyReadStatus reliabilityStatus =
      BacnetPropertyReadStatus::Skipped;
  uint32_t reliability = 0;
  BacnetPropertyReadStatus outOfServiceStatus =
      BacnetPropertyReadStatus::Skipped;
  bool outOfService = false;
  BacnetObjectHealthState state = BacnetObjectHealthState::Unknown;
};

bool bacnetDecodeStatusFlags(const BacnetValue& value,
                             BacnetStatusFlags& flags);
const char* bacnetEventStateText(uint32_t eventState);
const char* bacnetReliabilityText(uint32_t reliability);
BacnetObjectHealthState bacnetDeriveObjectHealthState(
    const BacnetObjectStatus& status,
    bool presentValueRequired = true);

static constexpr uint32_t kBacnetDefaultReadTimeoutMs = 1000;

struct BacnetSubscribeOptions {
  uint32_t fallbackPollMs = 10000;
  uint32_t timeoutMs = kBacnetDefaultReadTimeoutMs;
  bool immediateFirstRead = true;
  bool notifyOnStatusChange = true;
};

enum class BacnetSubscriptionNotificationReason : uint8_t {
  None = 0,
  FirstValue = 1,
  ValueChanged = 2,
  StatusChanged = 4,
};

inline BacnetSubscriptionNotificationReason operator|(
    BacnetSubscriptionNotificationReason left,
    BacnetSubscriptionNotificationReason right) {
  return static_cast<BacnetSubscriptionNotificationReason>(
      static_cast<uint8_t>(left) | static_cast<uint8_t>(right));
}

inline bool bacnetSubscriptionReasonContains(
    BacnetSubscriptionNotificationReason reasons,
    BacnetSubscriptionNotificationReason testReason) {
  return (static_cast<uint8_t>(reasons) & static_cast<uint8_t>(testReason)) !=
         0;
}

struct BacnetSubscriptionNotification {
  BacnetObjectId objectId;
  BacnetPropertyId propertyId = BacnetPropertyId::ObjectName;
  uint32_t arrayIndex = kBacnetNoArrayIndex;
  BacnetDeviceSessionReadStatus status = BacnetDeviceSessionReadStatus::Skipped;
  const BacnetValue* value = nullptr;
  bool hasValue = false;
  bool firstValue = false;
  bool valueChanged = false;
  bool statusChanged = false;
  BacnetSubscriptionNotificationReason reason =
      BacnetSubscriptionNotificationReason::None;
  void* userData = nullptr;
};

struct BacnetPropertyListReadResult {
  BacnetPropertyReadStatus status = BacnetPropertyReadStatus::Skipped;
  uint32_t advertised = 0;
  size_t stored = 0;
  bool truncated = false;
};

struct BacnetPropertyReadResult {
  BacnetPropertyId propertyId = BacnetPropertyId::ObjectName;
  BacnetPropertyReadStatus status = BacnetPropertyReadStatus::Skipped;
  BacnetValue value;
};

struct BacnetPropertyReadAllResult {
  size_t requested = 0;
  size_t attempted = 0;
  size_t stored = 0;
  size_t acked = 0;
  size_t failed = 0;
  bool truncated = false;
};

using BacnetSubscriptionCallback =
    void (*)(const BacnetSubscriptionNotification& notification);

class BacnetPropertySubscription {
 public:
  BacnetPropertySubscription(BacnetDeviceSession& session,
                             BacnetObjectId objectId,
                             BacnetPropertyId propertyId,
                             uint32_t arrayIndex,
                             BacnetSubscribeOptions options,
                             BacnetSubscriptionCallback callback = nullptr,
                             void* userData = nullptr);

  BacnetPropertySubscription(const BacnetPropertySubscription&) = delete;
  BacnetPropertySubscription& operator=(const BacnetPropertySubscription&) =
      delete;

  BacnetPropertySubscription(BacnetPropertySubscription&& other) noexcept;
  BacnetPropertySubscription& operator=(
      BacnetPropertySubscription&& other) noexcept;
  ~BacnetPropertySubscription();

  BacnetObjectId objectId() const;
  BacnetPropertyId propertyId() const;
  uint32_t arrayIndex() const;

  bool active() const;
  bool inFlight() const;
  bool hasValue() const;
  const BacnetValue& lastValue() const;
  BacnetDeviceSessionReadStatus lastStatus() const;
  uint32_t lastUpdateMs() const;
  BacnetSubscriptionNotificationReason lastNotificationReason() const;

  void stop();
  void requestRefresh();

 private:
  friend class BacnetDeviceSession;
  friend class BacnetProperty;

  enum class PollTrigger : uint8_t {
    None,
    Initial,
    Fallback,
    Refresh,
  };

  bool isDue(uint32_t nowMs) const;
  void scheduleNextPoll(uint32_t nowMs);
  void clearInFlightState();
  void moveFrom(BacnetPropertySubscription& other);

  BacnetDeviceSession* session_ = nullptr;
  BacnetObjectId objectId_;
  BacnetPropertyId propertyId_ = BacnetPropertyId::ObjectName;
  uint32_t arrayIndex_ = kBacnetNoArrayIndex;
  BacnetSubscribeOptions options_;
  BacnetSubscriptionCallback callback_ = nullptr;
  void* userData_ = nullptr;

  bool active_ = true;
  bool inFlight_ = false;
  bool hasValue_ = false;
  bool refreshRequested_ = false;
  bool initialReadPending_ = false;
  bool hasTerminalStatus_ = false;
  BacnetValue lastValue_;
  BacnetDeviceSessionReadStatus lastStatus_ = BacnetDeviceSessionReadStatus::Skipped;
  uint32_t lastUpdateMs_ = 0;
  uint32_t nextPollAtMs_ = 0;
  BacnetSubscriptionNotificationReason lastNotificationReason_ =
      BacnetSubscriptionNotificationReason::None;

  uint8_t inFlightInvokeId_ = 0;
  unsigned long inFlightStartedAt_ = 0;
  PollTrigger inFlightTrigger_ = PollTrigger::None;
};

class BacnetDeviceSession {
 public:
  static constexpr uint32_t kDefaultReadTimeoutMs = kBacnetDefaultReadTimeoutMs;

  BacnetDeviceSession(BacnetClient& client, uint32_t deviceInstance,
                      IPAddress address,
                      uint16_t port = BacnetClient::kDefaultPort);

  BacnetClient& client();
  const BacnetClient& client() const;
  uint32_t deviceInstance() const;
  IPAddress address() const;
  uint16_t port() const;
  BacnetObjectId deviceObject() const;
  BacnetRemoteObject object(BacnetObjectId objectId);
  BacnetRemoteObject object(BacnetObjectType objectType,
                            uint32_t objectInstance);

  BacnetDeviceSessionReadStatus readProperty(
      BacnetObjectId object, BacnetPropertyId property, BacnetValue& value,
      uint32_t timeoutMs = kDefaultReadTimeoutMs,
      uint32_t arrayIndex = kBacnetNoArrayIndex);
  BacnetDeviceSessionReadStatus readProperty(
      BacnetObjectType objectType, uint32_t objectInstance,
      BacnetPropertyId property, BacnetValue& value,
      uint32_t timeoutMs = kDefaultReadTimeoutMs,
      uint32_t arrayIndex = kBacnetNoArrayIndex);
  BacnetObjectHealthState readObjectStatus(
      BacnetObjectId object,
      BacnetObjectStatus& status,
      uint32_t timeoutMs = kDefaultReadTimeoutMs,
      bool presentValueRequired = true);
  BacnetObjectHealthState readObjectStatus(
      BacnetObjectType objectType,
      uint32_t objectInstance,
      BacnetObjectStatus& status,
      uint32_t timeoutMs = kDefaultReadTimeoutMs,
      bool presentValueRequired = true);
    BacnetPropertyListReadResult readPropertyList(
      BacnetObjectId object,
      BacnetPropertyId* properties,
      size_t propertyCapacity,
      uint32_t timeoutMs = kDefaultReadTimeoutMs);
    BacnetPropertyListReadResult readPropertyList(
      BacnetObjectType objectType,
      uint32_t objectInstance,
      BacnetPropertyId* properties,
      size_t propertyCapacity,
      uint32_t timeoutMs = kDefaultReadTimeoutMs);
    BacnetPropertyReadAllResult readAllProperties(
      BacnetObjectId object,
      const BacnetPropertyId* properties,
      size_t propertyCount,
      BacnetPropertyReadResult* results,
      size_t resultCapacity,
      uint32_t timeoutMs = kDefaultReadTimeoutMs);
    BacnetPropertyReadAllResult readAllProperties(
      BacnetObjectType objectType,
      uint32_t objectInstance,
      const BacnetPropertyId* properties,
      size_t propertyCount,
      BacnetPropertyReadResult* results,
      size_t resultCapacity,
      uint32_t timeoutMs = kDefaultReadTimeoutMs);
  BacnetObjectScanResult scanObjectList(
      const BacnetObjectScanOptions& options,
      BacnetScannedObject* results,
      size_t resultCapacity);
  bool beginObjectListScan(BacnetObjectListScanJob& job,
                           const BacnetObjectScanOptions& options,
                           BacnetScannedObject* results,
                           size_t resultCapacity,
                           uint32_t nowMs = millis());
  void pollObjectListScan(BacnetObjectListScanJob& job,
                          uint32_t nowMs = millis());
  void cancelObjectListScan(BacnetObjectListScanJob& job);
  void poll(BacnetPropertySubscription& subscription,
            uint32_t nowMs = millis());
  void poll(BacnetPropertySubscription* subscriptions,
            size_t count,
            uint32_t nowMs = millis());

 private:
  friend class BacnetPropertySubscription;
  friend class BacnetObjectListScanJob;

  static const char* subscriptionPollTriggerText(
      BacnetPropertySubscription::PollTrigger trigger);
  static bool bacnetValueEquals(const BacnetValue& left,
                                const BacnetValue& right);
    BacnetPropertyReadStatus readPropertyDetailed(
      const BacnetPropertyRequest& request,
      BacnetValue& value,
      uint32_t timeoutMs,
      uint32_t& errorClass,
      uint32_t& errorCode);
  bool tryStartObjectListScanRead(BacnetObjectListScanJob& job,
                                  const BacnetPropertyRequest& request,
                                  BacnetObjectListScanPhase phase,
                                  uint32_t nowMs);
  void pollInFlightObjectListScan(uint32_t nowMs);
  void finishObjectListScanRead(BacnetObjectListScanJob& job,
                                BacnetDeviceSessionReadStatus status,
                                const BacnetValue* value,
                                uint32_t nowMs);
  void advanceObjectListScan(BacnetObjectListScanJob& job, uint32_t nowMs);
  void completeObjectListScan(BacnetObjectListScanJob& job);
  void failObjectListScan(BacnetObjectListScanJob& job,
                          BacnetDeviceSessionReadStatus status);
  void releaseObjectListScan(BacnetObjectListScanJob& job);
  void pollInFlightSubscription(uint32_t nowMs);
  void tryStartSubscriptionPoll(BacnetPropertySubscription& subscription,
                                uint32_t nowMs);
  void finishSubscriptionPoll(BacnetPropertySubscription& subscription,
                              BacnetDeviceSessionReadStatus status,
                              const BacnetValue* value,
                              uint32_t nowMs);
  void releaseSubscription(BacnetPropertySubscription& subscription);
  uint8_t allocateInvokeId();

  BacnetClient& client_;
  uint32_t deviceInstance_ = 0;
  IPAddress address_;
  uint16_t port_ = BacnetClient::kDefaultPort;
  uint8_t nextInvokeId_ = 1;
  BacnetPropertySubscription* inFlightSubscription_ = nullptr;
  BacnetObjectListScanJob* inFlightObjectListScan_ = nullptr;
  size_t roundRobinSubscriptionIndex_ = 0;
};

struct BacnetObjectScanOptions {
  const BacnetObjectType* objectTypes = nullptr;
  size_t objectTypeCount = 0;
  uint32_t maxObjectListEntries = 600;
  uint32_t readTimeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs;
  bool readObjectName = true;
  bool readDescription = true;
  bool readPresentValue = true;

  bool acceptsObjectType(BacnetObjectId objectId) const;
  bool acceptsObjectType(BacnetObjectType objectType) const;
};

template <size_t N>
inline void bacnetSetObjectTypeFilter(
    BacnetObjectScanOptions& options,
    const BacnetObjectType (&objectTypes)[N]) {
  options.objectTypes = objectTypes;
  options.objectTypeCount = N;
}

inline void bacnetClearObjectTypeFilter(BacnetObjectScanOptions& options) {
  options.objectTypes = nullptr;
  options.objectTypeCount = 0;
}

struct BacnetScannedObject {
  BacnetObjectId objectId;
  BacnetDeviceSessionReadStatus objectNameStatus =
      BacnetDeviceSessionReadStatus::Skipped;
  BacnetValue objectName;
  BacnetDeviceSessionReadStatus descriptionStatus =
      BacnetDeviceSessionReadStatus::Skipped;
  BacnetValue description;
  BacnetDeviceSessionReadStatus presentValueStatus =
      BacnetDeviceSessionReadStatus::Skipped;
  BacnetValue presentValue;
};

struct BacnetObjectScanResult {
  BacnetDeviceSessionReadStatus objectListCountStatus =
      BacnetDeviceSessionReadStatus::Skipped;
  uint32_t objectListCount = 0;
  uint32_t inspected = 0;
  size_t found = 0;
  size_t stored = 0;
  bool truncated = false;
};

struct BacnetObjectListScanProgress {
  BacnetObjectListScanJobStatus status = BacnetObjectListScanJobStatus::Idle;
  BacnetObjectListScanPhase phase = BacnetObjectListScanPhase::Idle;
  uint32_t currentIndex = 0;
  bool requestInFlight = false;
  BacnetObjectScanResult summary;
};

class BacnetObjectListScanJob {
 public:
  BacnetObjectListScanJob() = default;

  BacnetObjectListScanJob(const BacnetObjectListScanJob&) = delete;
  BacnetObjectListScanJob& operator=(const BacnetObjectListScanJob&) = delete;

  BacnetObjectListScanJobStatus status() const;
  BacnetObjectListScanPhase phase() const;
  bool isIdle() const;
  bool isActive() const;
  bool isComplete() const;
  bool isFailed() const;
  bool isCancelled() const;
  bool isTerminal() const;
  bool requestInFlight() const;
  uint32_t currentIndex() const;
  const BacnetObjectScanResult& summary() const;
  BacnetObjectListScanProgress progress() const;

 private:
  friend class BacnetDeviceSession;

  void clear();
  void clearInFlightState();

  BacnetDeviceSession* session_ = nullptr;
  BacnetObjectScanOptions options_;
  BacnetScannedObject* results_ = nullptr;
  size_t resultCapacity_ = 0;
  BacnetObjectScanResult result_;
  BacnetObjectListScanJobStatus status_ = BacnetObjectListScanJobStatus::Idle;
  BacnetObjectListScanPhase phase_ = BacnetObjectListScanPhase::Idle;
  uint32_t currentIndex_ = 0;
  uint32_t maxIndex_ = 0;
  bool requestInFlight_ = false;
  uint8_t inFlightInvokeId_ = 0;
  uint32_t inFlightStartedAtMs_ = 0;
  BacnetPropertyRequest inFlightRequest_;
  BacnetValue inFlightValue_;
  BacnetObjectId currentObject_;
  size_t currentStoreIndex_ = 0;
  bool hasCurrentStoreIndex_ = false;
  bool truncationLogged_ = false;
};
