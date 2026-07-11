// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "portable/BacnetProtocol.h"
#include "portable/BacnetDisplayText.h"
#include "portable/BacnetRuntime.h"

int main() {
  uint8_t frame[BacnetProtocol::kWhoIsRequestSize] = {};
  const BacnetValue value{};
  return BacnetProtocol::buildWhoIsRequest(frame, sizeof(frame)) == sizeof(frame) &&
             bacnetObjectTypePrefix(BacnetObjectType::AnalogValue)[0] == 'A' &&
             !bacnetValueInRange(value, 0.0f, 1.0f)
           ? 0
           : 1;
}
