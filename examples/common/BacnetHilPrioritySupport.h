// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <BacnetRemoteObject.h>

#include <cmath>

namespace bacnet_example::hil {

// Internal HIL semantics shared by the Arduino and native runners. Callers
// retain explicit write, readback, timing, output, and cleanup control.
inline bool valuesEqual(const BacnetValue& left, const BacnetValue& right) {
  if (left.type != right.type) return false;
  if (left.type == BacnetValueType::Real) {
    return std::fabs(left.realValue - right.realValue) < 0.001F;
  }
  if (left.type == BacnetValueType::Enumerated ||
      left.type == BacnetValueType::Unsigned) {
    return left.unsignedValue == right.unsignedValue;
  }
  return left.type == BacnetValueType::Null;
}

inline bool selectTemporaryValue(BacnetRemoteObject& object,
                                 BacnetObjectType type,
                                 const BacnetValue& current,
                                 uint32_t timeoutMs,
                                 BacnetValue& value) {
  value = BacnetValue{};
  if (type == BacnetObjectType::AnalogValue && current.type == BacnetValueType::Real) {
    value.type = BacnetValueType::Real;
    value.realValue = 12.5F;
    return true;
  }
  if (type == BacnetObjectType::BinaryValue &&
      (current.type == BacnetValueType::Enumerated || current.type == BacnetValueType::Unsigned) &&
      current.unsignedValue <= 1) {
    value.type = current.type;
    value.unsignedValue = current.unsignedValue == 0 ? 1 : 0;
    return true;
  }
  if (type != BacnetObjectType::MultiStateValue ||
      (current.type != BacnetValueType::Enumerated && current.type != BacnetValueType::Unsigned)) {
    return false;
  }
  BacnetValue states;
  if (object.readProperty(BacnetPropertyId::NumberOfStates, states, timeoutMs) != BacnetDeviceSessionReadStatus::Ack ||
      (states.type != BacnetValueType::Enumerated && states.type != BacnetValueType::Unsigned) ||
      states.unsignedValue < 2 || current.unsignedValue == 0 ||
      current.unsignedValue > states.unsignedValue) {
    return false;
  }
  value.type = current.type;
  value.unsignedValue = current.unsignedValue == 1 ? 2 : 1;
  return true;
}

}  // namespace bacnet_example::hil
