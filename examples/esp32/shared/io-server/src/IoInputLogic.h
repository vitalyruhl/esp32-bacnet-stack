// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <cmath>
#include <cstdint>

namespace io_example {

constexpr uint32_t kReliabilityNoFaultDetected = 0;
constexpr uint32_t kReliabilityNoSensor = 1;
constexpr uint8_t kEventStateNormal = 0;
constexpr uint8_t kEventStateFault = 1;

struct InputHealth {
  bool fault = false;
  bool outOfService = false;
  uint32_t reliability = kReliabilityNoFaultDetected;
  uint8_t eventState = kEventStateNormal;
};

inline InputHealth inputHealth(bool enabled, bool valid) {
  if (!enabled)
    return {false, true, kReliabilityNoFaultDetected, kEventStateNormal};
  if (!valid)
    return {true, false, kReliabilityNoSensor, kEventStateFault};
  return {};
}

inline bool scaleAdcPercent(int raw,
                            int rawMinimum,
                            int rawMaximum,
                            bool inverted,
                            float technicalMinimum,
                            float technicalMaximum,
                            float& result) {
  if (rawMinimum == rawMaximum || !std::isfinite(technicalMinimum) ||
      !std::isfinite(technicalMaximum) || technicalMinimum > technicalMaximum) {
    return false;
  }
  const int lower = rawMinimum < rawMaximum ? rawMinimum : rawMaximum;
  const int upper = rawMinimum < rawMaximum ? rawMaximum : rawMinimum;
  const int clampedRaw = raw < lower ? lower : (raw > upper ? upper : raw);
  float fraction = static_cast<float>(clampedRaw - rawMinimum) /
                   static_cast<float>(rawMaximum - rawMinimum);
  if (inverted)
    fraction = 1.0F - fraction;
  result = technicalMinimum + fraction * (technicalMaximum - technicalMinimum);
  if (!std::isfinite(result))
    return false;
  if (result < technicalMinimum)
    result = technicalMinimum;
  if (result > technicalMaximum)
    result = technicalMaximum;
  return true;
}

class DebouncedButton {
public:
  void begin(bool rawLevel, uint32_t nowMs) {
    stableLevel_ = rawLevel;
    candidateLevel_ = rawLevel;
    changedAtMs_ = nowMs;
    initialized_ = true;
  }

  void update(bool rawLevel, uint32_t nowMs, uint32_t debounceMs) {
    if (!initialized_) {
      begin(rawLevel, nowMs);
      return;
    }
    if (rawLevel != candidateLevel_) {
      candidateLevel_ = rawLevel;
      changedAtMs_ = nowMs;
    }
    if (candidateLevel_ != stableLevel_ && nowMs - changedAtMs_ >= debounceMs)
      stableLevel_ = candidateLevel_;
  }

  bool stableLevel() const {
    return stableLevel_;
  }

private:
  bool initialized_ = false;
  bool stableLevel_ = true;
  bool candidateLevel_ = true;
  uint32_t changedAtMs_ = 0;
};

} // namespace io_example
