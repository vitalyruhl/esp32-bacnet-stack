// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "BacnetClient.h"

#include <cstddef>
#include <cstdint>

class BacnetRemoteObject;
struct BacnetObjectScanOptions;
struct BacnetObjectScanResult;
struct BacnetScannedObject;

enum class BacnetDeviceSessionReadStatus : uint8_t {
  Ack,
  Error,
  Timeout,
  SendFailed,
  Skipped,
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
  BacnetObjectScanResult scanObjectList(
      const BacnetObjectScanOptions& options,
      BacnetScannedObject* results,
      size_t resultCapacity);

 private:
  uint8_t allocateInvokeId();

  BacnetClient& client_;
  uint32_t deviceInstance_ = 0;
  IPAddress address_;
  uint16_t port_ = BacnetClient::kDefaultPort;
  uint8_t nextInvokeId_ = 1;
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
