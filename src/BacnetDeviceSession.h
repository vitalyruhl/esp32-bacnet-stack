// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "BacnetClient.h"

#include <cstddef>
#include <cstdint>

class BacnetDeviceSession;
class BacnetProperty;
class BacnetRemoteObject;
class BacnetPropertySubscription;
struct BacnetObjectScanOptions;
struct BacnetObjectScanResult;
struct BacnetScannedObject;

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
  BacnetObjectScanResult scanObjectList(
      const BacnetObjectScanOptions& options,
      BacnetScannedObject* results,
      size_t resultCapacity);
  void poll(BacnetPropertySubscription& subscription,
            uint32_t nowMs = millis());
  void poll(BacnetPropertySubscription* subscriptions,
            size_t count,
            uint32_t nowMs = millis());

 private:
  friend class BacnetPropertySubscription;

  static const char* subscriptionPollTriggerText(
      BacnetPropertySubscription::PollTrigger trigger);
  static bool bacnetValueEquals(const BacnetValue& left,
                                const BacnetValue& right);
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
