// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <Arduino.h>
#include <BacnetDisplayText.h>

String shortenBacnetLabel(const String& label);
String valueTextIfAck(BacnetDeviceSessionReadStatus status,
                      const BacnetValue& value);
String propertyValueTextIfAck(BacnetPropertyReadStatus status,
                              const BacnetValue& value);
String objectLabel(const BacnetScannedObject& scanned);
String bacnetObjectDisplayName(const BacnetObjectId& object);
String statusFlagsSummary(const BacnetObjectStatus& status);
String enumPropertySummary(BacnetPropertyReadStatus status, const char* text);
String boolPropertySummary(BacnetPropertyReadStatus status, bool value);
String bacnetScanTerminalStatus(const BacnetObjectScanResult& scan);
String ageSummary(uint32_t timestampMs);
