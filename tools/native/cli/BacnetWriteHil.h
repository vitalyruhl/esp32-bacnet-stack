// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "BacnetRemoteObject.h"

#include <cstdint>

enum class BacnetWriteHilStage : uint8_t {
  Complete,
  InvalidConfiguration,
  Read,
  ValueSelection,
  Write,
  Readback,
  Relinquish,
  Reset,
};

struct BacnetWriteHilTarget {
  BacnetObjectType type = BacnetObjectType::AnalogValue;
  uint32_t instance = 0;
  bool writableReset = false;
};

struct BacnetWriteHilOptions {
  uint8_t priority = 8;
  uint32_t timeoutMs = BacnetDeviceSession::kDefaultReadTimeoutMs;
};

struct BacnetWriteHilResult {
  BacnetWriteHilStage stage = BacnetWriteHilStage::InvalidConfiguration;
  BacnetDeviceSessionWriteStatus writeStatus = BacnetDeviceSessionWriteStatus::Ack;
  BacnetDeviceSessionWriteStatus relinquishStatus = BacnetDeviceSessionWriteStatus::Ack;
  BacnetPriorityRelinquishResult reset;
  BacnetValue originalPresentValue;
  BacnetValue temporaryValue;
  BacnetValue slotEight;
  BacnetValue slotSixteen;
  bool cleanupAttempted = false;

  bool succeeded() const { return stage == BacnetWriteHilStage::Complete; }
};

const char* bacnetWriteHilStageText(BacnetWriteHilStage stage);
bool bacnetWriteHilSelectTemporaryValue(BacnetRemoteObject& object,
                                        BacnetObjectType type,
                                        const BacnetValue& current,
                                        uint32_t timeoutMs,
                                        BacnetValue& value);
BacnetWriteHilResult bacnetWriteHilRunTarget(BacnetDeviceSession& session,
                                             const BacnetWriteHilTarget& target,
                                             const BacnetWriteHilOptions& options);
