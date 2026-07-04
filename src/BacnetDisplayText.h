// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "BacnetDeviceSession.h"

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
  return objectType ==
           static_cast<uint16_t>(BacnetObjectType::MultiStateInput) ||
         objectType ==
           static_cast<uint16_t>(BacnetObjectType::MultiStateOutput) ||
         objectType == static_cast<uint16_t>(BacnetObjectType::MultiStateValue);
}

inline bool bacnetIsProcessObject(uint16_t objectType) {
  return bacnetIsAnalogProcessObject(objectType) ||
         bacnetIsBinaryProcessObject(objectType) ||
         bacnetIsMultiStateProcessObject(objectType);
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
