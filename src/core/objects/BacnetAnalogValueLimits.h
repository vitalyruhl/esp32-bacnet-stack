// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <cmath>
#include <cstdint>

// Read-only BACnet state mapping; it deliberately does not send notifications.
enum class BacnetAnalogValueLimitState : uint8_t {
  Normal,
  LowWarning,
  HighWarning,
  LowFault,
  HighFault,
  SensorFault,
};

struct BacnetAnalogValueLimitConfig {
  float errorMinimum = 0.0F;
  float warningMinimum = 0.0F;
  float warningMaximum = 0.0F;
  float errorMaximum = 0.0F;
  float hysteresis = 0.0F;
};

struct BacnetAnalogValueLimitResult {
  BacnetAnalogValueLimitState state = BacnetAnalogValueLimitState::Normal;
  uint8_t eventState = 0;
  uint8_t reliability = 0;
  bool inAlarm = false;
  bool fault = false;
};

inline bool bacnetAnalogValueLimitConfigIsValid(const BacnetAnalogValueLimitConfig& config) {
  return std::isfinite(config.errorMinimum) && std::isfinite(config.warningMinimum) &&
         std::isfinite(config.warningMaximum) && std::isfinite(config.errorMaximum) &&
         std::isfinite(config.hysteresis) && config.hysteresis >= 0.0F &&
         config.errorMinimum <= config.warningMinimum &&
         config.warningMinimum < config.warningMaximum &&
         config.warningMaximum <= config.errorMaximum;
}

inline BacnetAnalogValueLimitResult bacnetAnalogValueLimitEvaluate(
  float value,
  bool sensorValid,
  const BacnetAnalogValueLimitConfig& config,
  BacnetAnalogValueLimitState previous = BacnetAnalogValueLimitState::Normal) {
  if (!sensorValid || !std::isfinite(value))
    return {BacnetAnalogValueLimitState::SensorFault, 1, 1, false, true};
  const float lowWarning = config.warningMinimum -
                           ((previous == BacnetAnalogValueLimitState::LowWarning || previous == BacnetAnalogValueLimitState::LowFault) ? config.hysteresis : 0.0F);
  const float highWarning = config.warningMaximum +
                            ((previous == BacnetAnalogValueLimitState::HighWarning || previous == BacnetAnalogValueLimitState::HighFault) ? config.hysteresis : 0.0F);
  if (value < config.errorMinimum)
    return {BacnetAnalogValueLimitState::LowFault, 4, 3, false, true};
  if (value > config.errorMaximum)
    return {BacnetAnalogValueLimitState::HighFault, 3, 2, false, true};
  if (value < lowWarning)
    return {BacnetAnalogValueLimitState::LowWarning, 4, 0, true, false};
  if (value > highWarning)
    return {BacnetAnalogValueLimitState::HighWarning, 3, 0, true, false};
  return {};
}
