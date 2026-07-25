// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetDemoFormat.h"

String shortenBacnetLabel(const String& label) {
  static constexpr size_t kMaxLabelLength = 36;
  if (label.length() <= kMaxLabelLength) {
    return label;
  }
  return label.substring(0, kMaxLabelLength - 3) + "...";
}

String valueTextIfAck(BacnetDeviceSessionReadStatus status,
                      const BacnetValue& value) {
  if (status == BacnetDeviceSessionReadStatus::Ack) {
    return value.displayText();
  }
  return bacnetReadStatusText(status);
}

String propertyValueTextIfAck(BacnetPropertyReadStatus status,
                              const BacnetValue& value) {
  if (status == BacnetPropertyReadStatus::Ack) {
    return value.displayText();
  }
  return bacnetPropertyReadStatusText(status);
}

String objectLabel(const BacnetScannedObject& scanned) {
  const String objectName =
    valueTextIfAck(scanned.objectNameStatus, scanned.objectName);
  if (scanned.objectNameStatus == BacnetDeviceSessionReadStatus::Ack &&
      objectName.length() > 0) {
    return objectName;
  }

  const String description =
    valueTextIfAck(scanned.descriptionStatus, scanned.description);
  if (scanned.descriptionStatus == BacnetDeviceSessionReadStatus::Ack &&
      description.length() > 0) {
    return description;
  }

  return String(bacnetObjectTypePrefix(scanned.objectId.type)) +
         String(scanned.objectId.instance);
}

String bacnetObjectDisplayName(const BacnetObjectId& object) {
  String name = bacnetObjectTypePrefix(object.type);
  name += String(object.instance);
  return name;
}

String statusFlagsSummary(const BacnetObjectStatus& status) {
  if (status.statusFlagsStatus != BacnetPropertyReadStatus::Ack) {
    return bacnetPropertyReadStatusText(status.statusFlagsStatus);
  }

  String text;
  text += status.statusFlags.inAlarm ? "alarm" : "no-alarm";
  text += ",";
  text += status.statusFlags.fault ? "fault" : "no-fault";
  text += ",";
  text += status.statusFlags.overridden ? "overridden" : "normal";
  text += ",";
  text += status.statusFlags.outOfService ? "oos" : "in-service";
  return text;
}

String enumPropertySummary(BacnetPropertyReadStatus status, const char* text) {
  if (status == BacnetPropertyReadStatus::Ack) {
    return text != nullptr ? text : "";
  }
  return bacnetPropertyReadStatusText(status);
}

String boolPropertySummary(BacnetPropertyReadStatus status, bool value) {
  if (status == BacnetPropertyReadStatus::Ack) {
    return value ? "true" : "false";
  }
  return bacnetPropertyReadStatusText(status);
}

String bacnetScanTerminalStatus(const BacnetObjectScanResult& scan) {
  if (scan.objectListCountStatus != BacnetDeviceSessionReadStatus::Ack) {
    String status = "Object-list unavailable: ";
    status += bacnetReadStatusText(scan.objectListCountStatus);
    return status;
  }
  if (scan.found == 0) {
    return "Scan completed with zero accepted process objects";
  }
  if (scan.stored == 0) {
    return "Scan completed with no displayable process objects";
  }

  String status = "Scan complete: ";
  status += String(static_cast<unsigned>(scan.stored));
  status += " process objects";
  if (scan.truncated) {
    status += " (truncated)";
  }
  return status;
}

String ageSummary(uint32_t timestampMs) {
  if (timestampMs == 0) {
    return "never";
  }
  const uint32_t ageMs = millis() - timestampMs;
  if (ageMs < 1000) {
    return String(ageMs) + " ms ago";
  }
  return String(ageMs / 1000) + " s ago";
}
