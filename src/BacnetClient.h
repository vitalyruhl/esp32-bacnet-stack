// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>

#include "BacnetLogger.h"

#include <cstddef>
#include <cstdint>

struct BacnetIAmDevice {
  IPAddress address;
  uint32_t deviceInstance = 0;
  uint32_t maxApduLengthAccepted = 0;
  uint8_t segmentationSupported = 0;
  uint16_t vendorId = 0;
};

struct BacnetObjectId {
  uint16_t type = 0;
  uint32_t instance = 0;
};

enum class BacnetObjectType : uint16_t {
  AnalogInput = 0,
  AnalogOutput = 1,
  AnalogValue = 2,
  BinaryInput = 3,
  BinaryOutput = 4,
  BinaryValue = 5,
  Device = 8,
  MultiStateInput = 13,
  MultiStateOutput = 14,
  MultiStateValue = 19,
};

inline const char* bacnetObjectTypeText(BacnetObjectType objectType) {
  switch (objectType) {
    case BacnetObjectType::AnalogInput:
      return "analog-input";
    case BacnetObjectType::AnalogOutput:
      return "analog-output";
    case BacnetObjectType::AnalogValue:
      return "analog-value";
    case BacnetObjectType::BinaryInput:
      return "binary-input";
    case BacnetObjectType::BinaryOutput:
      return "binary-output";
    case BacnetObjectType::BinaryValue:
      return "binary-value";
    case BacnetObjectType::MultiStateInput:
      return "multi-state-input";
    case BacnetObjectType::MultiStateOutput:
      return "multi-state-output";
    case BacnetObjectType::MultiStateValue:
      return "multi-state-value";
    case BacnetObjectType::Device:
      return "device";
  }
  return "object";
}

inline const char* bacnetObjectTypeText(uint16_t objectType) {
  return bacnetObjectTypeText(static_cast<BacnetObjectType>(objectType));
}

enum class BacnetPropertyId : uint32_t {
  Description = 28,
  EventState = 36,
  FirmwareRevision = 44,
  ModelName = 70,
  NumberOfStates = 74,
  ObjectList = 76,
  ObjectName = 77,
  ObjectType = 79,
  OutOfService = 81,
  PresentValue = 85,
  PriorityArray = 87,
  Reliability = 103,
  RelinquishDefault = 104,
  StateText = 110,
  StatusFlags = 111,
  PropertyList = 371,
  VendorName = 121,
};

static constexpr uint32_t kBacnetNoArrayIndex = 0xFFFFFFFF;

struct BacnetPropertyRequest {
  BacnetObjectId object;
  BacnetPropertyId property = BacnetPropertyId::ObjectName;
  uint32_t arrayIndex = kBacnetNoArrayIndex;
};

enum class BacnetValueType : uint8_t {
  Empty,
  Null,
  Boolean,
  Unsigned,
  Signed,
  Real,
  BitString,
  Enumerated,
  CharacterString,
  ObjectIdentifier,
  ObjectIdentifierList,
  Error,
  Unsupported,
};

struct BacnetValue {
  static constexpr size_t kMaxTextLength = 512;

  BacnetValueType type = BacnetValueType::Empty;
  char text[kMaxTextLength] = {};
  size_t textLength = 0;
  bool booleanValue = false;
  uint32_t unsignedValue = 0;
  int32_t signedValue = 0;
  float realValue = 0.0f;
  uint32_t bitStringValue = 0;
  uint8_t bitStringBitCount = 0;
  BacnetObjectId objectValue;

  const char* displayText() const {
    return text;
  }
};

enum class BacnetReadPropertyPollStatus {
  None,
  Ack,
  Error,
  Reject,
  Abort,
  DecodeError,
};

class BacnetClient {
 public:
  static constexpr uint16_t kDefaultPort = 47808;
  static constexpr size_t kWhoIsRequestSize = 8;
  static constexpr size_t kMaxReadPropertyRequestSize = 25;
  static constexpr uint32_t kNoArrayIndex = kBacnetNoArrayIndex;

  BacnetClient() = default;

  bool begin(uint16_t localPort = kDefaultPort);
  void end();

  bool isRunning() const;
  uint16_t localPort() const;
  BacnetLogger& logger();
  const BacnetLogger& logger() const;

  bool sendWhoIs(IPAddress address = IPAddress(255, 255, 255, 255),
                 uint16_t port = kDefaultPort);
  bool pollIAm(BacnetIAmDevice& device);
  bool sendReadProperty(IPAddress address, const BacnetPropertyRequest& request,
                        uint8_t invokeId = 1,
                        uint16_t port = kDefaultPort);
  bool sendReadProperty(IPAddress address, BacnetObjectId object,
                        BacnetPropertyId property, uint8_t invokeId = 1,
                        uint16_t port = kDefaultPort,
                        uint32_t arrayIndex = kNoArrayIndex);
  bool pollReadProperty(BacnetValue& value, uint8_t expectedInvokeId,
                        const BacnetPropertyRequest& expectedRequest);
  bool pollReadProperty(BacnetValue& value, uint8_t expectedInvokeId,
                        BacnetPropertyId expectedProperty);
  void logReadPropertyTimeout(uint8_t invokeId,
                              const BacnetPropertyRequest& request);
  BacnetReadPropertyPollStatus pollReadPropertyStatus(
      BacnetValue& value, uint8_t expectedInvokeId,
      const BacnetPropertyRequest& expectedRequest);
    BacnetReadPropertyPollStatus pollReadPropertyStatus(
      BacnetValue& value, uint8_t expectedInvokeId,
      const BacnetPropertyRequest& expectedRequest,
      uint32_t* errorClass,
      uint32_t* errorCode);
  BacnetReadPropertyPollStatus pollReadPropertyStatus(
      BacnetValue& value, uint8_t expectedInvokeId,
      BacnetPropertyId expectedProperty);
    BacnetReadPropertyPollStatus pollReadPropertyStatus(
      BacnetValue& value, uint8_t expectedInvokeId,
      BacnetPropertyId expectedProperty,
      uint32_t* errorClass,
      uint32_t* errorCode);

  static size_t buildWhoIsRequest(uint8_t* buffer, size_t bufferSize);
  static bool parseIAmResponse(const uint8_t* buffer, size_t length,
                               BacnetIAmDevice& device);
  static size_t buildReadPropertyRequest(uint8_t* buffer, size_t bufferSize,
                                         const BacnetPropertyRequest& request,
                                         uint8_t invokeId = 1);
  static size_t buildReadPropertyRequest(uint8_t* buffer, size_t bufferSize,
                                         BacnetObjectId object,
                                         BacnetPropertyId property,
                                         uint8_t invokeId = 1,
                                         uint32_t arrayIndex = kNoArrayIndex);
  static bool parseReadPropertyAck(const uint8_t* buffer, size_t length,
                                   uint8_t expectedInvokeId,
                                   const BacnetPropertyRequest& expectedRequest,
                                   BacnetValue& value);
  static bool parseReadPropertyAck(const uint8_t* buffer, size_t length,
                                   uint8_t expectedInvokeId,
                                   BacnetPropertyId expectedProperty,
                                   BacnetValue& value);
  static bool parseReadPropertyError(const uint8_t* buffer, size_t length,
                                     uint8_t expectedInvokeId,
                                     BacnetValue& value);
  static bool parseReadPropertyError(const uint8_t* buffer, size_t length,
                                     uint8_t expectedInvokeId,
                                     BacnetValue& value,
                                     uint32_t* errorClass,
                                     uint32_t* errorCode);

 private:
  static constexpr size_t kMaxDiscoveryPacketSize = 512;

  WiFiUDP udp_;
  BacnetLogger logger_;
  bool running_ = false;
  uint16_t localPort_ = kDefaultPort;
};
