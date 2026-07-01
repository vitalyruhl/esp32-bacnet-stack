// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "BacnetDeviceSession.h"

#include <cstdint>

class BacnetProperty {
 public:
  BacnetProperty(BacnetDeviceSession& session, BacnetObjectId objectId,
                 BacnetPropertyId propertyId,
                 uint32_t arrayIndex = kBacnetNoArrayIndex);

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

 private:
  BacnetDeviceSession* session_;
  BacnetObjectId objectId_;
  BacnetPropertyId propertyId_ = BacnetPropertyId::ObjectName;
  uint32_t arrayIndex_ = kBacnetNoArrayIndex;
};

class BacnetRemoteObject {
 public:
  BacnetRemoteObject(BacnetDeviceSession& session, BacnetObjectId objectId);

  BacnetObjectId objectId() const;
  BacnetProperty property(
      BacnetPropertyId id,
      uint32_t arrayIndex = kBacnetNoArrayIndex) const;
  BacnetDeviceSessionReadStatus readProperty(
      BacnetPropertyId id, BacnetValue& value,
      uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs,
      uint32_t arrayIndex = kBacnetNoArrayIndex) const;
  BacnetDeviceSessionReadStatus readObjectName(
      BacnetValue& value,
      uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;
  BacnetDeviceSessionReadStatus readDescription(
      BacnetValue& value,
      uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;
  BacnetDeviceSessionReadStatus readPresentValue(
      BacnetValue& value,
      uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;
  BacnetDeviceSessionReadStatus readPropertyList(
      BacnetValue& value,
      uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs) const;

 private:
  BacnetDeviceSession* session_;
  BacnetObjectId objectId_;
};
