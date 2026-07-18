// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "IoInputLogic.h"

#include <cmath>

namespace {

bool testAdcScaling() {
  float value = 0.0F;
  return io_example::scaleAdcPercent(0, 0, 4095, false, 0.0F, 100.0F, value) &&
         value == 0.0F &&
         io_example::scaleAdcPercent(4095, 0, 4095, false, 0.0F, 100.0F, value) &&
         value == 100.0F &&
         io_example::scaleAdcPercent(9999, 4095, 0, true, 0.0F, 100.0F, value) &&
         value == 100.0F &&
         !io_example::scaleAdcPercent(1, 10, 10, false, 0.0F, 100.0F, value) &&
         !io_example::scaleAdcPercent(1, 0, 10, false, 100.0F, 0.0F, value);
}

bool testDs18b20FaultMapping() {
  const io_example::InputHealth sensorMissing = io_example::inputHealth(true, false);
  const io_example::InputHealth disabled = io_example::inputHealth(false, false);
  return sensorMissing.fault && !sensorMissing.outOfService &&
         sensorMissing.reliability == io_example::kReliabilityNoSensor &&
         sensorMissing.eventState == io_example::kEventStateFault &&
         !disabled.fault && disabled.outOfService &&
         disabled.reliability == io_example::kReliabilityNoFaultDetected;
}

bool testButtonDebouncing() {
  io_example::DebouncedButton button;
  button.begin(true, 0);
  button.update(false, 10, 35);
  if (!button.stableLevel()) return false;
  button.update(true, 20, 35);
  button.update(false, 30, 35);
  button.update(false, 64, 35);
  if (!button.stableLevel()) return false;
  button.update(false, 65, 35);
  return !button.stableLevel();
}

} // namespace

int main() {
  return testAdcScaling() && testDs18b20FaultMapping() && testButtonDebouncing() ? 0 : 1;
}
