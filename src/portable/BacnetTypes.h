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

namespace BacnetEngineeringUnits {
constexpr uint32_t Pascals = 53;
constexpr uint32_t DegreesCelsius = 62;
constexpr uint32_t PercentRelativeHumidity = 29;
constexpr uint32_t Seconds = 73;
constexpr uint32_t Percent = 98;
} // namespace BacnetEngineeringUnits

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
  ApduTimeout = 10,
  ApplicationSoftwareVersion = 12,
  CovIncrement = 22,
  Description = 28,
  DeviceAddressBinding = 30,
  EventState = 36,
  FirmwareRevision = 44,
  MaxApduLengthAccepted = 62,
  MaxPresentValue = 65,
  MinPresentValue = 69,
  ModelName = 70,
  NumberOfApduRetries = 73,
  NumberOfStates = 74,
  ObjectIdentifier = 75,
  ObjectList = 76,
  ObjectName = 77,
  ObjectType = 79,
  OutOfService = 81,
  PresentValue = 85,
  PriorityArray = 87,
  Polarity = 84,
  ProtocolObjectTypesSupported = 96,
  ProtocolServicesSupported = 97,
  ProtocolVersion = 98,
  Reliability = 103,
  RelinquishDefault = 104,
  Resolution = 106,
  StateText = 110,
  StatusFlags = 111,
  SystemStatus = 112,
  Units = 117,
  VendorIdentifier = 120,
  VendorName = 121,
  ProtocolRevision = 139,
  DatabaseRevision = 155,
  SegmentationSupported = 107,
  PropertyList = 371,
};

static constexpr uint32_t kBacnetNoArrayIndex = 0xFFFFFFFFUL;

struct BacnetPropertyRequest {
  BacnetObjectId object;
  BacnetPropertyId property = BacnetPropertyId::ObjectName;
  uint32_t arrayIndex = kBacnetNoArrayIndex;
};

struct BacnetWritePropertyOptions {
  uint32_t arrayIndex = kBacnetNoArrayIndex;
  bool hasPriority = false;
  uint8_t priority = 0;
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
  NotCommandable,
  Unsupported,
  PropertyIdentifierList,
};

// Stable BACnet error names used by protocol decoders and presentation layers.
// Keep error class/code interpretation here so native and Arduino clients do
// not describe the same protocol response differently.
inline const char* bacnetErrorClassName(uint32_t errorClass) {
  switch (errorClass) {
    case 1:
      return "object";
    case 2:
      return "property";
    default:
      return "error";
  }
}

inline const char* bacnetErrorCodeName(uint32_t errorCode) {
  switch (errorCode) {
    case 26:
      return "optional-functionality-not-supported";
    case 31:
      return "unknown-object";
    case 32:
      return "unknown-property";
    case 42:
      return "invalid-array-index";
    default:
      return "error";
  }
}

// Stable technical names for BACnet values. Presentation layers may format a
// value differently, but must not maintain a second mapping for its type.
inline const char* bacnetValueTypeName(BacnetValueType type) {
  switch (type) {
    case BacnetValueType::Empty:
      return "empty";
    case BacnetValueType::Null:
      return "null";
    case BacnetValueType::Boolean:
      return "boolean";
    case BacnetValueType::Unsigned:
      return "unsigned";
    case BacnetValueType::Signed:
      return "signed";
    case BacnetValueType::Real:
      return "real";
    case BacnetValueType::BitString:
      return "bit-string";
    case BacnetValueType::Enumerated:
      return "enumerated";
    case BacnetValueType::CharacterString:
      return "string";
    case BacnetValueType::ObjectIdentifier:
      return "object-id";
    case BacnetValueType::ObjectIdentifierList:
      return "object-id-list";
    case BacnetValueType::Error:
      return "error";
    case BacnetValueType::NotCommandable:
      return "not-commandable";
    case BacnetValueType::Unsupported:
      return "unsupported";
    case BacnetValueType::PropertyIdentifierList:
      return "property-id-list";
  }
  return "unknown";
}

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

struct BacnetPriorityArray {
  static constexpr size_t kSlotCount = 16;

  BacnetValue slots[kSlotCount];
  bool present[kSlotCount] = {};
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

enum class BacnetWritePropertyPollStatus : uint8_t {
  None,
  Ack,
  Error,
  NotCommandable,
  Reject,
  Abort,
  DecodeError,
  Disabled,
  InvalidArgument,
  UnsupportedValue,
  SendFailed,
};

enum class BacnetWritePropertyResponseKind : uint8_t {
  Unrelated,
  Ack,
  Error,
  Reject,
  Abort,
};

struct BacnetCovNotification {
  uint32_t processId = 0;
  BacnetObjectId object;
  BacnetPropertyId property = BacnetPropertyId::PresentValue;
  uint32_t arrayIndex = kBacnetNoArrayIndex;
  BacnetValue value;
};

enum class BacnetSubscribeCovResponseKind : uint8_t {
  None,
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
    case BacnetPropertyId::ObjectIdentifier:
      return "objectIdentifier";
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
    case BacnetPropertyId::Polarity:
      return "polarity";
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

struct BacnetWhoIsRequest {
  bool hasDeviceInstanceRange = false;
  uint32_t lowDeviceInstance = 0;
  uint32_t highDeviceInstance = 0;

  bool includes(uint32_t deviceInstance) const {
    return !hasDeviceInstanceRange ||
           (deviceInstance >= lowDeviceInstance &&
            deviceInstance <= highDeviceInstance);
  }
};

struct BacnetConfirmedRequestHeader {
  uint8_t invokeId = 0;
  uint8_t serviceChoice = 0;
  size_t apduOffset = 0;
};

enum class BacnetConfirmedRequestParseStatus : uint8_t {
  Unrelated,
  Malformed,
  Confirmed,
};

struct BacnetReadPropertyRequestHeader {
  uint8_t invokeId = 0;
  BacnetPropertyRequest request;
};

struct BacnetWritePropertyRequestHeader {
  uint8_t invokeId = 0;
  BacnetPropertyRequest request;
  BacnetValue value;
  bool hasPriority = false;
  uint8_t priority = 0;
};

enum class BacnetReadPropertyRequestParseStatus : uint8_t {
  Unrelated,
  Malformed,
  ReadProperty,
};

enum class BacnetWritePropertyRequestParseStatus : uint8_t {
  Unrelated,
  Malformed,
  WriteProperty,
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
