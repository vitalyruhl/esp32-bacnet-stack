// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetDemoFallbackObjects.h"

namespace {

BacnetObjectId typedObject(BacnetObjectType type, uint32_t instance) {
  return BacnetObjectId{static_cast<uint16_t>(type), instance};
}

} // namespace

BacnetObjectId bacnetDemoFallbackAnalogObject(uint32_t instance) {
  return typedObject(BacnetObjectType::AnalogValue, instance);
}

BacnetObjectId bacnetDemoFallbackBinaryObject(uint32_t instance) {
  return typedObject(BacnetObjectType::BinaryValue, instance);
}

BacnetObjectId bacnetDemoFallbackMultiStateObject(uint32_t instance) {
  return typedObject(BacnetObjectType::MultiStateValue, instance);
}
