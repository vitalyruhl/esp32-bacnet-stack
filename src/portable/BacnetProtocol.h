// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "BacnetTypes.h"

using BacnetObjectListEntryProvider = bool (*)(const void* context,
                                               size_t index,
                                               BacnetObjectId& object);
using BacnetPropertyListEntryProvider = bool (*)(const void* context,
                                                 size_t index,
                                                 BacnetPropertyId& property);

class BacnetProtocol {
public:
  static constexpr size_t kWhoIsRequestSize = 8;
  static constexpr size_t kMaxIAmResponseSize = 26;
  static constexpr size_t kRejectResponseSize = 9;
  static constexpr size_t kMaxReadPropertyRequestSize = 25;
  static constexpr size_t kMaxWritePropertyRequestSize = 544;
  static constexpr size_t kMaxSubscribeCovRequestSize = 32;

  static size_t buildWhoIsRequest(uint8_t* buffer, size_t bufferSize);
  static bool parseWhoIsRequest(const uint8_t* buffer,
                                size_t length,
                                BacnetWhoIsRequest& request);
  static BacnetConfirmedRequestParseStatus parseConfirmedRequestHeader(
    const uint8_t* buffer,
    size_t length,
    BacnetConfirmedRequestHeader& header);
  static BacnetReadPropertyRequestParseStatus parseReadPropertyRequest(
    const uint8_t* buffer,
    size_t length,
    BacnetReadPropertyRequestHeader& request);
  static size_t buildReadPropertyAck(uint8_t* buffer,
                                     size_t bufferSize,
                                     const BacnetReadPropertyRequestHeader& request,
                                     const BacnetValue& value);
  static size_t buildReadPropertyError(uint8_t* buffer,
                                       size_t bufferSize,
                                       uint8_t invokeId,
                                       uint32_t errorClass,
                                       uint32_t errorCode);
  static size_t buildReadPropertyObjectListAck(
    uint8_t* buffer,
    size_t bufferSize,
    const BacnetReadPropertyRequestHeader& request,
    const BacnetObjectId* objects,
    size_t objectCount);
  static size_t buildReadPropertyObjectListAck(
    uint8_t* buffer,
    size_t bufferSize,
    const BacnetReadPropertyRequestHeader& request,
    size_t objectCount,
    BacnetObjectListEntryProvider objectAt,
    const void* context);
  static size_t buildReadPropertyPropertyListAck(
    uint8_t* buffer,
    size_t bufferSize,
    const BacnetReadPropertyRequestHeader& request,
    const BacnetPropertyId* properties,
    size_t propertyCount);
  static size_t buildReadPropertyPropertyListAck(
    uint8_t* buffer,
    size_t bufferSize,
    const BacnetReadPropertyRequestHeader& request,
    size_t propertyCount,
    BacnetPropertyListEntryProvider propertyAt,
    const void* context);
  static size_t buildReadPropertyEmptyListAck(
    uint8_t* buffer,
    size_t bufferSize,
    const BacnetReadPropertyRequestHeader& request);
  static size_t buildIAmResponse(uint8_t* buffer,
                                 size_t bufferSize,
                                 const BacnetIAmDeviceInfo& device);
  static size_t buildRejectResponse(uint8_t* buffer,
                                    size_t bufferSize,
                                    uint8_t invokeId,
                                    uint8_t reason);
  static size_t encodeApplicationValue(uint8_t* buffer, size_t bufferSize, const BacnetValue& value);
  static size_t buildSubscribeCovRequest(uint8_t* buffer, size_t bufferSize, uint32_t processId, BacnetObjectId object, uint32_t lifetimeSeconds, bool issueConfirmedNotifications = false);
  static BacnetSubscribeCovResponseKind classifySubscribeCovResponse(
    const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, uint8_t* rejectReason = nullptr);
  static const char* rejectReasonText(uint8_t rejectReason);
  static bool parseCovNotification(const uint8_t* buffer, size_t length, BacnetCovNotification& notification);
  static bool parseIAmResponse(const uint8_t* buffer, size_t length, BacnetIAmDeviceInfo& device);
  static size_t buildReadPropertyRequest(uint8_t* buffer, size_t bufferSize, const BacnetPropertyRequest& request, uint8_t invokeId = 1);
  static size_t buildWritePropertyRequest(uint8_t* buffer, size_t bufferSize, const BacnetPropertyRequest& request, const BacnetValue& value, uint8_t invokeId = 1);
  static size_t buildWritePropertyRequest(uint8_t* buffer, size_t bufferSize, BacnetObjectId object, BacnetPropertyId property, const BacnetValue& value, const BacnetWritePropertyOptions& options, uint8_t invokeId = 1);
  static size_t buildReadPropertyRequest(uint8_t* buffer, size_t bufferSize, BacnetObjectId object, BacnetPropertyId property, uint8_t invokeId = 1, uint32_t arrayIndex = kBacnetNoArrayIndex);
  static bool parseReadPropertyAck(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, const BacnetPropertyRequest& expectedRequest, BacnetValue& value);
  static bool parseReadPropertyAck(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, BacnetPropertyId expectedProperty, BacnetValue& value);
  static bool parseReadPriorityArrayAck(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, const BacnetPropertyRequest& expectedRequest, BacnetPriorityArray& value);
  static bool parseReadPropertyError(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, BacnetValue& value);
  static bool parseReadPropertyError(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, BacnetValue& value, uint32_t* errorClass, uint32_t* errorCode);
  static BacnetReadPropertyResponseKind classifyReadPropertyResponse(
    const uint8_t* buffer, size_t length, uint8_t expectedInvokeId);
  static BacnetWritePropertyResponseKind classifyWritePropertyResponse(
    const uint8_t* buffer, size_t length, uint8_t expectedInvokeId);
};
