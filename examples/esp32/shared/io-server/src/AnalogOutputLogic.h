// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace io_example {

// Keep board policy in the ESP32 example. The portable server only commits an
// effective BACnet value before calling its output hook.
inline bool isSafeEsp32PwmPin(int pin) {
  if (pin < 0 || pin > 33) {
    return false;
  }
  switch (pin) {
    case 0:
    case 2:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 15:
      return false;
    default:
      return true;
  }
}

inline bool isEsp32DacPin(int pin) {
  return pin == 25 || pin == 26;
}

inline bool isUnownedOutputPin(int pin, const int* ownedPins, size_t ownedPinCount) {
  if (ownedPins == nullptr && ownedPinCount != 0U) {
    return false;
  }
  for (size_t index = 0; index < ownedPinCount; ++index) {
    if (ownedPins[index] == pin) {
      return false;
    }
  }
  return true;
}

inline bool scaleOutputPercentToPwm(float value,
                                    float minimum,
                                    float maximum,
                                    bool inverted,
                                    bool enabled,
                                    uint8_t& duty) {
  if (!std::isfinite(value) || !std::isfinite(minimum) || !std::isfinite(maximum) ||
      minimum >= maximum) {
    return false;
  }
  if (!enabled) {
    duty = 0;
    return true;
  }
  const float clamped = value < minimum ? minimum : (value > maximum ? maximum : value);
  float fraction = (clamped - minimum) / (maximum - minimum);
  if (inverted) {
    fraction = 1.0F - fraction;
  }
  duty = static_cast<uint8_t>(std::lround(fraction * 255.0F));
  return true;
}

} // namespace io_example
