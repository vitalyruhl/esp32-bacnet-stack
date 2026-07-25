// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "core/client/BacnetDeviceSession.h"
#include "core/protocol/BacnetTypes.h"
#include "support/BacnetFixedTextBuffer.h"

#include <cstdio>
#include <cstring>

inline const char* bacnetValueDisplayText(const BacnetValue& value,
                                          char* output,
                                          size_t outputCapacity) {
  if (output == nullptr || outputCapacity == 0) {
    return "";
  }

  switch (value.type) {
    case BacnetValueType::Null:
      std::snprintf(output, outputCapacity, "null");
      break;
    case BacnetValueType::Boolean:
      std::snprintf(output, outputCapacity, "%s", value.booleanValue ? "true" : "false");
      break;
    case BacnetValueType::Unsigned:
    case BacnetValueType::Enumerated:
      std::snprintf(output, outputCapacity, "%lu", static_cast<unsigned long>(value.unsignedValue));
      break;
    case BacnetValueType::Signed:
      std::snprintf(output, outputCapacity, "%ld", static_cast<long>(value.signedValue));
      break;
    case BacnetValueType::Real:
      std::snprintf(output, outputCapacity, "%.3f", static_cast<double>(value.realValue));
      break;
    case BacnetValueType::CharacterString: {
      const size_t count = value.textLength < outputCapacity - 1
                             ? value.textLength
                             : outputCapacity - 1;
      std::memcpy(output, value.text, count);
      output[count] = '\0';
      break;
    }
    case BacnetValueType::ObjectIdentifier:
      std::snprintf(output, outputCapacity, "%s%lu", bacnetObjectTypeText(value.objectValue.type), static_cast<unsigned long>(value.objectValue.instance));
      break;
    default:
      std::snprintf(output, outputCapacity, "%s", value.textLength > 0 ? value.displayText() : "empty");
      break;
  }
  return output;
}

inline const char* bacnetObjectTypePrefix(uint16_t objectType) {
  switch (static_cast<BacnetObjectType>(objectType)) {
    case BacnetObjectType::AnalogInput:
      return "AI";
    case BacnetObjectType::AnalogOutput:
      return "AO";
    case BacnetObjectType::AnalogValue:
      return "AV";
    case BacnetObjectType::BinaryInput:
      return "BI";
    case BacnetObjectType::BinaryOutput:
      return "BO";
    case BacnetObjectType::BinaryValue:
      return "BV";
    case BacnetObjectType::MultiStateInput:
      return "MI";
    case BacnetObjectType::MultiStateOutput:
      return "MO";
    case BacnetObjectType::MultiStateValue:
      return "MSV";
    case BacnetObjectType::Device:
      return "DEV";
    default:
      return "OBJ";
  }
}

inline const char* bacnetObjectTypePrefix(BacnetObjectType objectType) {
  return bacnetObjectTypePrefix(static_cast<uint16_t>(objectType));
}

inline bool bacnetIsAnalogProcessObject(uint16_t objectType) {
  return objectType == static_cast<uint16_t>(BacnetObjectType::AnalogInput) ||
         objectType == static_cast<uint16_t>(BacnetObjectType::AnalogOutput) ||
         objectType == static_cast<uint16_t>(BacnetObjectType::AnalogValue);
}

inline bool bacnetIsAnalogProcessObject(BacnetObjectType objectType) {
  return bacnetIsAnalogProcessObject(static_cast<uint16_t>(objectType));
}

inline bool bacnetIsBinaryProcessObject(uint16_t objectType) {
  return objectType == static_cast<uint16_t>(BacnetObjectType::BinaryInput) ||
         objectType == static_cast<uint16_t>(BacnetObjectType::BinaryOutput) ||
         objectType == static_cast<uint16_t>(BacnetObjectType::BinaryValue);
}

inline bool bacnetIsBinaryProcessObject(BacnetObjectType objectType) {
  return bacnetIsBinaryProcessObject(static_cast<uint16_t>(objectType));
}

inline bool bacnetIsMultiStateProcessObject(uint16_t objectType) {
  return objectType == static_cast<uint16_t>(BacnetObjectType::MultiStateInput) ||
         objectType == static_cast<uint16_t>(BacnetObjectType::MultiStateOutput) ||
         objectType == static_cast<uint16_t>(BacnetObjectType::MultiStateValue);
}

inline bool bacnetIsMultiStateProcessObject(BacnetObjectType objectType) {
  return bacnetIsMultiStateProcessObject(static_cast<uint16_t>(objectType));
}

inline bool bacnetIsProcessObject(uint16_t objectType) {
  return bacnetIsAnalogProcessObject(objectType) ||
         bacnetIsBinaryProcessObject(objectType) ||
         bacnetIsMultiStateProcessObject(objectType);
}

inline bool bacnetIsProcessObject(BacnetObjectType objectType) {
  return bacnetIsProcessObject(static_cast<uint16_t>(objectType));
}

inline bool bacnetValueAsFloat(const BacnetValue& value, float& output) {
  switch (value.type) {
    case BacnetValueType::Real:
      output = value.realValue;
      return true;
    case BacnetValueType::Unsigned:
    case BacnetValueType::Enumerated:
      output = static_cast<float>(value.unsignedValue);
      return true;
    case BacnetValueType::Signed:
      output = static_cast<float>(value.signedValue);
      return true;
    case BacnetValueType::Boolean:
      output = value.booleanValue ? 1.0f : 0.0f;
      return true;
    default:
      return false;
  }
}

inline bool bacnetValueInRange(const BacnetValue& value, float minValue, float maxValue) {
  float numericValue = 0.0f;
  return bacnetValueAsFloat(value, numericValue) && numericValue >= minValue &&
         numericValue <= maxValue;
}

inline const char* bacnetPropertyDisplayName(BacnetPropertyId propertyId) {
  switch (propertyId) {
    case BacnetPropertyId::PresentValue:
      return "present-value";
    case BacnetPropertyId::ObjectName:
      return "object-name";
    case BacnetPropertyId::Description:
      return "description";
    case BacnetPropertyId::StatusFlags:
      return "status-flags";
    case BacnetPropertyId::EventState:
      return "event-state";
    case BacnetPropertyId::Reliability:
      return "reliability";
    case BacnetPropertyId::OutOfService:
      return "out-of-service";
    case BacnetPropertyId::Units:
      return "units";
    case BacnetPropertyId::MinPresentValue:
      return "min-present-value";
    case BacnetPropertyId::MaxPresentValue:
      return "max-present-value";
    case BacnetPropertyId::Resolution:
      return "resolution";
    case BacnetPropertyId::CovIncrement:
      return "cov-increment";
    default:
      return "property";
  }
}

inline const char* bacnetSubscriptionReasonText(
  const BacnetSubscriptionNotification& notification) {
  if (notification.firstValue) {
    return "first";
  }
  if (notification.valueChanged) {
    return "changed";
  }
  if (notification.statusChanged) {
    return "status";
  }
  return "update";
}

inline void bacnetAppendObjectDisplayName(FixedTextBuffer& out,
                                          const BacnetObjectId& object) {
  out.append(bacnetObjectTypePrefix(object.type));
  out.appendFormat("%lu", static_cast<unsigned long>(object.instance));
}

inline const char* bacnetValueTextOrNull(
  BacnetDeviceSessionReadStatus status,
  const BacnetValue& value) {
  return status == BacnetDeviceSessionReadStatus::Ack && value.textLength > 0
           ? value.displayText()
           : nullptr;
}

inline const char* bacnetValueTextOrNull(BacnetPropertyReadStatus status,
                                         const BacnetValue& value) {
  return status == BacnetPropertyReadStatus::Ack && value.textLength > 0
           ? value.displayText()
           : nullptr;
}

inline const char* bacnetScannedObjectNameOrNull(
  const BacnetScannedObject& scanned) {
  return bacnetValueTextOrNull(scanned.objectNameStatus, scanned.objectName);
}

inline const char* bacnetScannedDescriptionOrNull(
  const BacnetScannedObject& scanned) {
  return bacnetValueTextOrNull(scanned.descriptionStatus, scanned.description);
}

inline const char* bacnetScannedLabelOrNull(
  const BacnetScannedObject& scanned) {
  const char* objectName = bacnetScannedObjectNameOrNull(scanned);
  if (objectName != nullptr && objectName[0] != '\0') {
    return objectName;
  }
  const char* description = bacnetScannedDescriptionOrNull(scanned);
  if (description != nullptr && description[0] != '\0') {
    return description;
  }
  return nullptr;
}

inline bool bacnetStatusHasAlarm(const BacnetObjectStatus& status) {
  return status.statusFlagsStatus == BacnetPropertyReadStatus::Ack &&
         status.statusFlags.inAlarm;
}

inline bool bacnetStatusHasFault(const BacnetObjectStatus& status) {
  return status.statusFlagsStatus == BacnetPropertyReadStatus::Ack &&
         status.statusFlags.fault;
}

inline bool bacnetStatusIsOutOfService(const BacnetObjectStatus& status) {
  if (status.outOfServiceStatus == BacnetPropertyReadStatus::Ack) {
    return status.outOfService;
  }
  return status.statusFlagsStatus == BacnetPropertyReadStatus::Ack &&
         status.statusFlags.outOfService;
}

inline bool bacnetStatusIsNormal(const BacnetObjectStatus& status) {
  return status.state == BacnetObjectHealthState::Normal &&
         !bacnetStatusHasAlarm(status) && !bacnetStatusHasFault(status) &&
         !bacnetStatusIsOutOfService(status);
}

inline void bacnetAppendStatusFlagsSummary(
  FixedTextBuffer& out,
  const BacnetObjectStatus& status) {
  if (status.statusFlagsStatus != BacnetPropertyReadStatus::Ack) {
    out.append(bacnetPropertyReadStatusText(status.statusFlagsStatus));
    return;
  }

  out.append(status.statusFlags.inAlarm ? "alarm" : "no-alarm");
  out.append(',');
  out.append(status.statusFlags.fault ? "fault" : "no-fault");
  out.append(',');
  out.append(status.statusFlags.overridden ? "overridden" : "normal");
  out.append(',');
  out.append(status.statusFlags.outOfService ? "oos" : "in-service");
}

inline void bacnetAppendEnumPropertySummary(FixedTextBuffer& out,
                                            BacnetPropertyReadStatus status,
                                            const char* text) {
  out.append(status == BacnetPropertyReadStatus::Ack
               ? (text != nullptr ? text : "")
               : bacnetPropertyReadStatusText(status));
}

inline void bacnetAppendBoolPropertySummary(FixedTextBuffer& out,
                                            BacnetPropertyReadStatus status,
                                            bool value) {
  out.append(status == BacnetPropertyReadStatus::Ack
               ? (value ? "true" : "false")
               : bacnetPropertyReadStatusText(status));
}

inline const char* bacnetCommonEngineeringUnitSymbol(uint32_t unitId) {
  switch (unitId) {
    case 2:
      return "mA";
    case 3:
      return "A";
    case 5:
      return "V";
    case 19:
      return "kWh";
    case 27:
      return "Hz";
    case 29:
      return "%RH";
    case 31:
      return "m";
    case 37:
      return "lx";
    case 47:
      return "W";
    case 48:
      return "kW";
    case 53:
      return "Pa";
    case 54:
      return "kPa";
    case 62:
      return "°C";
    case 63:
      return "K";
    case 64:
      return "°F";
    case 71:
      return "h";
    case 72:
      return "min";
    case 73:
      return "s";
    case 74:
      return "m/s";
    case 82:
      return "l";
    case 87:
      return "l/s";
    case 88:
      return "l/min";
    case 90:
      return "°";
    case 95:
      return "-";
    case 96:
      return "ppm";
    case 98:
      return "%";
    default:
      return nullptr;
  }
}
