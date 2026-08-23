// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <cmath>

// Maps a finite raw value to an engineering value. Input values outside the
// raw range are clamped to the corresponding engineering-range endpoint.
// Reversed raw and engineering ranges are valid. Invalid input produces 0.0F
// and returns false so callers never divide by zero or retain a stale value.
struct BacnetLinearScale {
  float rawMin = 0.0F;
  float rawMax = 1.0F;
  float valueMin = 0.0F;
  float valueMax = 1.0F;

  bool isValid() const {
    return std::isfinite(rawMin) && std::isfinite(rawMax) &&
           std::isfinite(valueMin) && std::isfinite(valueMax) &&
           rawMin != rawMax;
  }

  bool apply(float rawValue, float& value) const {
    if (!isValid() || !std::isfinite(rawValue)) {
      value = 0.0F;
      return false;
    }
    float ratio = (rawValue - rawMin) / (rawMax - rawMin);
    if (ratio < 0.0F) {
      ratio = 0.0F;
    } else if (ratio > 1.0F) {
      ratio = 1.0F;
    }
    value = valueMin + ratio * (valueMax - valueMin);
    if (!std::isfinite(value)) {
      value = 0.0F;
      return false;
    }
    return true;
  }
};
