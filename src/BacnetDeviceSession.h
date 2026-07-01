// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "BacnetClient.h"

#include <cstdint>

class BacnetRemoteObject;

enum class BacnetDeviceSessionReadStatus : uint8_t {
  Ack,
  Error,
  Timeout,
  SendFailed,
};

class BacnetDeviceSession {
 public:
  static constexpr uint32_t kDefaultReadTimeoutMs = 1000;

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

 private:
  uint8_t allocateInvokeId();

  BacnetClient& client_;
  uint32_t deviceInstance_ = 0;
  IPAddress address_;
  uint16_t port_ = BacnetClient::kDefaultPort;
  uint8_t nextInvokeId_ = 1;
};
