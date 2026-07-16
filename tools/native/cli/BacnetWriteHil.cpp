// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetWriteHil.h"

#include <chrono>
#include <cmath>
#include <thread>

namespace {

bool valueEquals(const BacnetValue& left, const BacnetValue& right) {
  if (left.type != right.type) {
    return false;
  }
  if (left.type == BacnetValueType::Real) {
    return std::fabs(left.realValue - right.realValue) < 0.001F;
  }
  if (left.type == BacnetValueType::Enumerated ||
      left.type == BacnetValueType::Unsigned) {
    return left.unsignedValue == right.unsignedValue;
  }
  return left.type == BacnetValueType::Null;
}

bool readSlot(BacnetRemoteObject& object, uint8_t priority, uint32_t timeoutMs,
              BacnetValue& value) {
  const BacnetPropertyReadStatus status =
    object.readPriorityArray(value, timeoutMs, priority);
  return status == BacnetPropertyReadStatus::Ack ||
         status == BacnetPropertyReadStatus::EmptyValue;
}

bool readPresentValue(BacnetRemoteObject& object, uint32_t timeoutMs,
                      BacnetValue& value) {
  return object.readPresentValue(value, timeoutMs) ==
         BacnetDeviceSessionReadStatus::Ack;
}

bool observePrioritySixteen(BacnetRemoteObject& object, uint32_t timeoutMs,
                            BacnetValue& slotSixteen) {
  for (uint8_t attempt = 0; attempt < 3; ++attempt) {
    BacnetValue presentValue;
    if (readSlot(object, 16, timeoutMs, slotSixteen) &&
        slotSixteen.type != BacnetValueType::Null &&
        readPresentValue(object, timeoutMs, presentValue) &&
        valueEquals(presentValue, slotSixteen)) {
      return true;
    }
    if (attempt < 2) {
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
  }
  return false;
}

void relinquishAfterFailure(BacnetRemoteObject& object, uint8_t priority,
                            uint32_t timeoutMs, BacnetWriteHilResult& result) {
  result.cleanupAttempted = true;
  result.relinquishStatus = object.relinquishPresentValue(priority, timeoutMs);
}

BacnetWriteHilResult fail(BacnetWriteHilResult result,
                          BacnetWriteHilStage stage,
                          BacnetRemoteObject& object,
                          const BacnetWriteHilOptions& options,
                          bool priorityActive) {
  result.stage = stage;
  if (priorityActive) {
    relinquishAfterFailure(object, options.priority, options.timeoutMs, result);
  }
  return result;
}

}  // namespace

const char* bacnetWriteHilStageText(BacnetWriteHilStage stage) {
  switch (stage) {
    case BacnetWriteHilStage::Complete: return "complete";
    case BacnetWriteHilStage::InvalidConfiguration: return "invalid-configuration";
    case BacnetWriteHilStage::Read: return "read";
    case BacnetWriteHilStage::ValueSelection: return "value-selection";
    case BacnetWriteHilStage::Write: return "write";
    case BacnetWriteHilStage::Readback: return "readback";
    case BacnetWriteHilStage::Relinquish: return "relinquish";
    case BacnetWriteHilStage::Reset: return "reset";
  }
  return "unknown";
}

bool bacnetWriteHilSelectTemporaryValue(BacnetRemoteObject& object,
                                        BacnetObjectType type,
                                        const BacnetValue& current,
                                        uint32_t timeoutMs,
                                        BacnetValue& value) {
  value = BacnetValue{};
  if (type == BacnetObjectType::AnalogValue &&
      current.type == BacnetValueType::Real) {
    value.type = BacnetValueType::Real;
    value.realValue = 12.5F;
    return true;
  }
  if (type == BacnetObjectType::BinaryValue &&
      (current.type == BacnetValueType::Enumerated ||
       current.type == BacnetValueType::Unsigned) &&
      current.unsignedValue <= 1) {
    value.type = current.type;
    value.unsignedValue = current.unsignedValue == 0 ? 1 : 0;
    return true;
  }
  if (type != BacnetObjectType::MultiStateValue ||
      (current.type != BacnetValueType::Enumerated &&
       current.type != BacnetValueType::Unsigned)) {
    return false;
  }
  BacnetValue states;
  if (object.readProperty(BacnetPropertyId::NumberOfStates, states, timeoutMs) !=
        BacnetDeviceSessionReadStatus::Ack ||
      (states.type != BacnetValueType::Enumerated &&
       states.type != BacnetValueType::Unsigned) ||
      states.unsignedValue < 2 || current.unsignedValue == 0 ||
      current.unsignedValue > states.unsignedValue) {
    return false;
  }
  value.type = current.type;
  value.unsignedValue = current.unsignedValue == 1 ? 2 : 1;
  return true;
}

BacnetWriteHilResult bacnetWriteHilRunTarget(
  BacnetDeviceSession& session, const BacnetWriteHilTarget& target,
  const BacnetWriteHilOptions& options) {
  BacnetWriteHilResult result;
  if (target.instance == 0 || options.priority == 0 || options.priority > 16 ||
      (target.type != BacnetObjectType::AnalogValue &&
       target.type != BacnetObjectType::BinaryValue &&
       target.type != BacnetObjectType::MultiStateValue)) {
    return result;
  }

  BacnetRemoteObject object = session.object(target.type, target.instance);
  if (!readPresentValue(object, options.timeoutMs, result.originalPresentValue) ||
      !readSlot(object, options.priority, options.timeoutMs, result.slotEight) ||
      result.slotEight.type != BacnetValueType::Null ||
      !readSlot(object, 16, options.timeoutMs, result.slotSixteen)) {
    return fail(result, BacnetWriteHilStage::Read, object, options, false);
  }
  if (!bacnetWriteHilSelectTemporaryValue(object, target.type,
                                          result.originalPresentValue,
                                          options.timeoutMs,
                                          result.temporaryValue)) {
    return fail(result, BacnetWriteHilStage::ValueSelection, object, options,
                false);
  }

  result.writeStatus = object.writePresentValue(
    result.temporaryValue, options.priority, options.timeoutMs);
  if (result.writeStatus != BacnetDeviceSessionWriteStatus::Ack) {
    return fail(result, BacnetWriteHilStage::Write, object, options, false);
  }
  bool priorityActive = true;
  BacnetValue presentValue;
  if (!readPresentValue(object, options.timeoutMs, presentValue) ||
      !valueEquals(presentValue, result.temporaryValue) ||
      !readSlot(object, options.priority, options.timeoutMs, result.slotEight) ||
      !valueEquals(result.slotEight, result.temporaryValue) ||
      !readSlot(object, 16, options.timeoutMs, result.slotSixteen) ||
      result.slotSixteen.type == BacnetValueType::Null) {
    return fail(result, BacnetWriteHilStage::Readback, object, options,
                priorityActive);
  }

  result.relinquishStatus = object.relinquishPresentValue(
    options.priority, options.timeoutMs);
  priorityActive = false;
  if (result.relinquishStatus != BacnetDeviceSessionWriteStatus::Ack) {
    return fail(result, BacnetWriteHilStage::Relinquish, object, options,
                priorityActive);
  }
  if (!readSlot(object, options.priority, options.timeoutMs, result.slotEight) ||
      result.slotEight.type != BacnetValueType::Null ||
      !readSlot(object, 16, options.timeoutMs, result.slotSixteen) ||
      result.slotSixteen.type == BacnetValueType::Null ||
      !readPresentValue(object, options.timeoutMs, presentValue) ||
      !valueEquals(presentValue, result.slotSixteen)) {
    return fail(result, BacnetWriteHilStage::Relinquish, object, options,
                priorityActive);
  }

  if (target.writableReset) {
    BacnetPriorityResetOptions resetOptions;
    resetOptions.skipMinimumOnOffPriority = true;
    result.reset = object.relinquishAllPriorities(resetOptions, options.timeoutMs);
    if (!result.reset.succeeded() || result.reset.completedPriorities != 15 ||
        result.reset.skippedPriority != kMinimumOnOffPriority ||
        !readSlot(object, options.priority, options.timeoutMs, result.slotEight) ||
        result.slotEight.type != BacnetValueType::Null ||
        !observePrioritySixteen(object, options.timeoutMs, result.slotSixteen)) {
      return fail(result, BacnetWriteHilStage::Reset, object, options,
                  priorityActive);
    }
  }

  result.stage = BacnetWriteHilStage::Complete;
  return result;
}
