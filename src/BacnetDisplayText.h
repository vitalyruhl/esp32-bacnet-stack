// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "BacnetDeviceSession.h"
#include "BacnetFixedTextBuffer.h"
#include "portable/BacnetDisplayText.h"

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
