// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "BacnetDeviceSession.h"

#include <cstdint>

class BacnetProperty {
public:
  BacnetProperty(BacnetDeviceSession& session, BacnetObjectId objectId, BacnetPropertyId propertyId, uint32_t arrayIndex = kBacnetNoArrayIndex);

  BacnetPropertyRequest request() const;
  BacnetObjectId objectId() const;
  BacnetPropertyId propertyId() const;
  uint32_t arrayIndex() const;
  BacnetPropertySubscription subscribe(
    BacnetSubscriptionCallback callback,
    void* userData = nullptr,
    const BacnetSubscribeOptions& options = BacnetSubscribeOptions{}) const;
  BacnetDeviceSessionReadStatus read(
    BacnetValue& value,
    uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;
  bool hasValue() const;
  BacnetValue lastValue() const;
  BacnetPropertyReadStatus lastStatus() const;
  uint32_t lastUpdateMs() const;
  uint32_t lastAttemptMs() const;
  uint32_t lastSuccessMs() const;

private:
  BacnetDeviceSession* session_;
  BacnetObjectId objectId_;
  BacnetPropertyId propertyId_ = BacnetPropertyId::ObjectName;
  uint32_t arrayIndex_ = kBacnetNoArrayIndex;
};

struct BacnetPriorityRelinquishResult {
  BacnetDeviceSessionWriteStatus status = BacnetDeviceSessionWriteStatus::Ack;
  uint8_t completedPriorities = 0;
  uint8_t failedPriority = 0;

  bool succeeded() const {
    return failedPriority == 0 &&
           completedPriorities == 16 &&
           status == BacnetDeviceSessionWriteStatus::Ack;
  }
};

class BacnetRemoteObject {
public:
  BacnetRemoteObject(BacnetDeviceSession& session, BacnetObjectId objectId);

  BacnetObjectId objectId() const;
  BacnetProperty property(
    BacnetPropertyId id,
    uint32_t arrayIndex = kBacnetNoArrayIndex) const;
  BacnetDeviceSessionReadStatus readProperty(
    BacnetPropertyId id, BacnetValue& value, uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs, uint32_t arrayIndex = kBacnetNoArrayIndex) const;
  BacnetDeviceSessionReadStatus readObjectName(
    BacnetValue& value,
    uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;
  BacnetDeviceSessionReadStatus readDescription(
    BacnetValue& value,
    uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;
  BacnetDeviceSessionReadStatus readPresentValue(
    BacnetValue& value,
    uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;
  BacnetPropertyReadStatus readPriorityArray(
    BacnetValue& value,
    uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs,
    uint32_t arrayIndex = kBacnetNoArrayIndex) const;
  BacnetPropertyReadStatus readPriorityArray(
    BacnetPriorityArray& value,
    uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;
  BacnetPropertyReadStatus readRelinquishDefault(
    BacnetValue& value,
    uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;
  BacnetDeviceSessionWriteStatus writePresentValue(
    const BacnetValue& value, uint8_t priority, uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;
  BacnetDeviceSessionWriteStatus relinquishPresentValue(
    uint8_t priority,
    uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;
  BacnetPriorityRelinquishResult relinquishAllPriorities(
    uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;
  BacnetDeviceSessionReadStatus readPropertyList(
    BacnetValue& value,
    uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;
  BacnetPropertyListReadResult readPropertyList(
    BacnetPropertyId* properties,
    size_t propertyCapacity,
    uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;
  BacnetPropertyReadAllResult readAllProperties(
    const BacnetPropertyId* properties,
    size_t propertyCount,
    BacnetPropertyReadResult* results,
    size_t resultCapacity,
    uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;
  BacnetPropertyReadAllResult readAllAdvertisedProperties(
    BacnetPropertyId* properties,
    size_t propertyCapacity,
    BacnetPropertyReadResult* results,
    size_t resultCapacity,
    uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;

private:
  BacnetDeviceSession* session_;
  BacnetObjectId objectId_;
};
