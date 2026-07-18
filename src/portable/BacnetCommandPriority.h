// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <cstddef>
#include <cstdint>

// Caller-owned BACnet command-priority storage. It deliberately has no
// transport or object-type dependency so Binary Output and future commandable
// object families share exactly the same effective-value selection.
template <typename T>
struct BacnetCommandPriority {
  static constexpr size_t kSlotCount = 16;

  T relinquishDefault{};
  T slots[kSlotCount]{};
  bool occupied[kSlotCount]{};

  T effectiveValue() const {
    for (size_t index = 0; index < kSlotCount; ++index) {
      if (occupied[index])
        return slots[index];
    }
    return relinquishDefault;
  }

  bool write(uint8_t priority, bool relinquish, T value = T{}) {
    if (priority == 0 || priority > kSlotCount)
      return false;
    const size_t index = priority - 1U;
    occupied[index] = !relinquish;
    if (!relinquish)
      slots[index] = value;
    return true;
  }
};
