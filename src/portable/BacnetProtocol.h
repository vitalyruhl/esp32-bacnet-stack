// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "BacnetTypes.h"

class BacnetProtocol {
public:
  static constexpr size_t kWhoIsRequestSize = 8;
  static constexpr size_t kMaxReadPropertyRequestSize = 25;
  static constexpr size_t kMaxSubscribeCovRequestSize = 32;

  static size_t buildWhoIsRequest(uint8_t* buffer, size_t bufferSize);
  static size_t buildSubscribeCovRequest(uint8_t* buffer, size_t bufferSize, uint32_t processId, BacnetObjectId object, uint32_t lifetimeSeconds);
  static BacnetSubscribeCovResponseKind classifySubscribeCovResponse(
    const uint8_t* buffer, size_t length, uint8_t expectedInvokeId);
  static bool parseCovNotification(const uint8_t* buffer, size_t length, BacnetCovNotification& notification);
  static bool parseIAmResponse(const uint8_t* buffer, size_t length, BacnetIAmDeviceInfo& device);
  static size_t buildReadPropertyRequest(uint8_t* buffer, size_t bufferSize, const BacnetPropertyRequest& request, uint8_t invokeId = 1);
  static size_t buildReadPropertyRequest(uint8_t* buffer, size_t bufferSize, BacnetObjectId object, BacnetPropertyId property, uint8_t invokeId = 1, uint32_t arrayIndex = kBacnetNoArrayIndex);
  static bool parseReadPropertyAck(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, const BacnetPropertyRequest& expectedRequest, BacnetValue& value);
  static bool parseReadPropertyAck(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, BacnetPropertyId expectedProperty, BacnetValue& value);
  static bool parseReadPropertyError(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, BacnetValue& value);
  static bool parseReadPropertyError(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, BacnetValue& value, uint32_t* errorClass, uint32_t* errorCode);
  static BacnetReadPropertyResponseKind classifyReadPropertyResponse(
    const uint8_t* buffer, size_t length, uint8_t expectedInvokeId);
};
