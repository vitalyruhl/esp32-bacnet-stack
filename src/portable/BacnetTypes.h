// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <cstddef>
#include <cstdint>

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
    case BacnetObjectType::Device:
      return "device";
    case BacnetObjectType::MultiStateInput:
      return "multi-state-input";
    case BacnetObjectType::MultiStateOutput:
      return "multi-state-output";
    case BacnetObjectType::MultiStateValue:
      return "multi-state-value";
  }
  return "object";
}

inline const char* bacnetObjectTypeText(uint16_t objectType) {
  return bacnetObjectTypeText(static_cast<BacnetObjectType>(objectType));
}

enum class BacnetPropertyId : uint32_t {
  ApplicationSoftwareVersion = 12,
  CovIncrement = 22,
  Description = 28,
  EventState = 36,
  FirmwareRevision = 44,
  MaxPresentValue = 65,
  MinPresentValue = 69,
  ModelName = 70,
  NumberOfStates = 74,
  ObjectList = 76,
  ObjectName = 77,
  ObjectType = 79,
  OutOfService = 81,
  PresentValue = 85,
  PriorityArray = 87,
  ProtocolVersion = 98,
  Reliability = 103,
  RelinquishDefault = 104,
  Resolution = 106,
  StateText = 110,
  StatusFlags = 111,
  Units = 117,
  VendorIdentifier = 120,
  VendorName = 121,
  ProtocolRevision = 139,
  PropertyList = 371,
};

static constexpr uint32_t kBacnetNoArrayIndex = 0xFFFFFFFFUL;

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

enum class BacnetReadPropertyPollStatus { None,
                                          Ack,
                                          Error,
                                          Reject,
                                          Abort,
                                          DecodeError };

enum class BacnetReadPropertyResponseKind : uint8_t {
  Unrelated,
  Ack,
  Error,
  Reject,
  Abort,
};

inline const char* bacnetPropertyName(BacnetPropertyId property) {
  switch (property) {
    case BacnetPropertyId::ApplicationSoftwareVersion:
      return "applicationSoftwareVersion";
    case BacnetPropertyId::CovIncrement:
      return "covIncrement";
    case BacnetPropertyId::Description:
      return "description";
    case BacnetPropertyId::EventState:
      return "eventState";
    case BacnetPropertyId::FirmwareRevision:
      return "firmwareRevision";
    case BacnetPropertyId::MaxPresentValue:
      return "maxPresentValue";
    case BacnetPropertyId::MinPresentValue:
      return "minPresentValue";
    case BacnetPropertyId::ModelName:
      return "modelName";
    case BacnetPropertyId::NumberOfStates:
      return "numberOfStates";
    case BacnetPropertyId::ObjectList:
      return "objectList";
    case BacnetPropertyId::ObjectName:
      return "objectName";
    case BacnetPropertyId::ObjectType:
      return "objectType";
    case BacnetPropertyId::OutOfService:
      return "outOfService";
    case BacnetPropertyId::PresentValue:
      return "presentValue";
    case BacnetPropertyId::ProtocolRevision:
      return "protocolRevision";
    case BacnetPropertyId::ProtocolVersion:
      return "protocolVersion";
    case BacnetPropertyId::PriorityArray:
      return "priorityArray";
    case BacnetPropertyId::PropertyList:
      return "propertyList";
    case BacnetPropertyId::Reliability:
      return "reliability";
    case BacnetPropertyId::RelinquishDefault:
      return "relinquishDefault";
    case BacnetPropertyId::Resolution:
      return "resolution";
    case BacnetPropertyId::StateText:
      return "stateText";
    case BacnetPropertyId::StatusFlags:
      return "statusFlags";
    case BacnetPropertyId::Units:
      return "units";
    case BacnetPropertyId::VendorIdentifier:
      return "vendorIdentifier";
    case BacnetPropertyId::VendorName:
      return "vendorName";
    default:
      return "property";
  }
}

struct BacnetIAmDeviceInfo {
  uint32_t deviceInstance = 0;
  uint32_t maxApduLengthAccepted = 0;
  uint8_t segmentationSupported = 0;
  uint16_t vendorId = 0;
};

struct BacnetIpEndpoint {
  uint8_t address[4] = {};
  uint16_t port = 47808;

  constexpr BacnetIpEndpoint() = default;
  constexpr BacnetIpEndpoint(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t endpointPort = 47808)
      : address{a, b, c, d}, port(endpointPort) {}

  bool isZero() const {
    return address[0] == 0 && address[1] == 0 && address[2] == 0 && address[3] == 0;
  }
};
