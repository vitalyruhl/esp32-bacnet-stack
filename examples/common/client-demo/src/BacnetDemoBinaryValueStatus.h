// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <BacnetRemoteObject.h>

#include <cstdint>
#include <memory>

enum class BacnetDemoBinaryValueState : uint8_t {
  Unknown,
  Inactive,
  Active,
};

class BacnetDemoBinaryValueStatus {
public:
  BacnetDemoBinaryValueStatus(uint32_t objectInstance,
                              uint32_t pollMs,
                              uint32_t timeoutMs)
      : objectInstance_(objectInstance), pollMs_(pollMs), timeoutMs_(timeoutMs) {}

  void configure(uint32_t objectInstance) {
    if (objectInstance_ == objectInstance) {
      return;
    }
    subscription_.reset();
    objectInstance_ = objectInstance;
    state_ = BacnetDemoBinaryValueState::Unknown;
    lastReadStatus_ = BacnetDeviceSessionReadStatus::Skipped;
    unsupportedValue_ = false;
    initialReadPending_ = true;
    session_ = nullptr;
  }

  void reset() {
    subscription_.reset();
    state_ = BacnetDemoBinaryValueState::Unknown;
    lastReadStatus_ = BacnetDeviceSessionReadStatus::Skipped;
    unsupportedValue_ = false;
    initialReadPending_ = true;
    session_ = nullptr;
  }

  uint32_t objectInstance() const {
    return objectInstance_;
  }
  BacnetDemoBinaryValueState state() const {
    return state_;
  }
  bool isActive() const {
    return state_ == BacnetDemoBinaryValueState::Active;
  }
  BacnetDeviceSessionReadStatus lastReadStatus() const {
    return lastReadStatus_;
  }

  const char* stateText() const {
    switch (state_) {
      case BacnetDemoBinaryValueState::Active:
        return "active";
      case BacnetDemoBinaryValueState::Inactive:
        return "inactive";
      case BacnetDemoBinaryValueState::Unknown:
        return "unknown";
    }
    return "unknown";
  }

  const char* detailText() const {
    if (state_ == BacnetDemoBinaryValueState::Unknown &&
        lastReadStatus_ == BacnetDeviceSessionReadStatus::Ack &&
        unsupportedValue_) {
      return "unsupported value";
    }
    return bacnetReadStatusText(lastReadStatus_);
  }

  void setup(BacnetDeviceSession& session) {
    subscription_.reset();
    session_ = &session;
    BacnetSubscribeOptions options;
    options.preferCov = false;
    options.fallbackPollMs = pollMs_;
    options.timeoutMs = timeoutMs_;
    options.immediateFirstRead = initialReadPending_;
    options.notifyOnStatusChange = true;
    subscription_.reset(new BacnetPropertySubscription(
      session.object(BacnetObjectType::BinaryValue, objectInstance_)
        .property(BacnetPropertyId::PresentValue)
        .subscribe(onSubscriptionUpdate, this, options)));
  }

  void poll(BacnetDeviceSession& session, uint32_t now) {
    if (session_ != &session || !subscription_) {
      setup(session);
    }
    session.poll(*subscription_, now);
  }

  void readBackAfterWrite(BacnetDeviceSession& session,
                          BacnetDeviceSessionWriteStatus writeStatus) {
    if (writeStatus != BacnetDeviceSessionWriteStatus::Ack) {
      return;
    }

    subscription_.reset();
    session_ = &session;
    BacnetValue value;
    const BacnetDeviceSessionReadStatus readStatus =
      session.object(BacnetObjectType::BinaryValue, objectInstance_)
        .readPresentValue(value, timeoutMs_);
    applyReadResult(readStatus, value);
  }

  void applyReadResult(BacnetDeviceSessionReadStatus status,
                       const BacnetValue& value) {
    lastReadStatus_ = status;
    unsupportedValue_ = false;
    initialReadPending_ = false;
    if (status != BacnetDeviceSessionReadStatus::Ack) {
      state_ = BacnetDemoBinaryValueState::Unknown;
      return;
    }

    if (value.type == BacnetValueType::Boolean) {
      state_ = value.booleanValue ? BacnetDemoBinaryValueState::Active
                                  : BacnetDemoBinaryValueState::Inactive;
      return;
    }

    if ((value.type == BacnetValueType::Enumerated ||
         value.type == BacnetValueType::Unsigned) &&
        value.unsignedValue <= 1U) {
      state_ = value.unsignedValue == 1U ? BacnetDemoBinaryValueState::Active
                                         : BacnetDemoBinaryValueState::Inactive;
      return;
    }

    state_ = BacnetDemoBinaryValueState::Unknown;
    unsupportedValue_ = true;
  }

private:
  static void onSubscriptionUpdate(
    const BacnetSubscriptionNotification& notification) {
    auto* status = static_cast<BacnetDemoBinaryValueStatus*>(notification.userData);
    if (status == nullptr) {
      return;
    }
    const BacnetValue emptyValue;
    status->applyReadResult(
      notification.status,
      notification.hasValue && notification.value != nullptr
        ? *notification.value
        : emptyValue);
  }

  uint32_t objectInstance_ = 0;
  uint32_t pollMs_ = 0;
  uint32_t timeoutMs_ = 0;
  BacnetDemoBinaryValueState state_ = BacnetDemoBinaryValueState::Unknown;
  BacnetDeviceSessionReadStatus lastReadStatus_ =
    BacnetDeviceSessionReadStatus::Skipped;
  bool unsupportedValue_ = false;
  bool initialReadPending_ = true;
  BacnetDeviceSession* session_ = nullptr;
  std::unique_ptr<BacnetPropertySubscription> subscription_;
};
