// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "BacnetTypes.h"

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
      return "MV";
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

inline bool bacnetIsBinaryProcessObject(uint16_t objectType) {
  return objectType == static_cast<uint16_t>(BacnetObjectType::BinaryInput) ||
         objectType == static_cast<uint16_t>(BacnetObjectType::BinaryOutput) ||
         objectType == static_cast<uint16_t>(BacnetObjectType::BinaryValue);
}

inline bool bacnetIsMultiStateProcessObject(uint16_t objectType) {
  return objectType == static_cast<uint16_t>(BacnetObjectType::MultiStateInput) ||
         objectType == static_cast<uint16_t>(BacnetObjectType::MultiStateOutput) ||
         objectType == static_cast<uint16_t>(BacnetObjectType::MultiStateValue);
}

inline bool bacnetIsProcessObject(uint16_t objectType) {
  return bacnetIsAnalogProcessObject(objectType) ||
         bacnetIsBinaryProcessObject(objectType) ||
         bacnetIsMultiStateProcessObject(objectType);
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
