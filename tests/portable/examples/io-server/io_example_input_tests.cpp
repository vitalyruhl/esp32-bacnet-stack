// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "AnalogOutputLogic.h"
#include "IoInputLogic.h"
#include "LedOutputLogic.h"

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
  if (!button.stableLevel())
    return false;
  button.update(true, 20, 35);
  button.update(false, 30, 35);
  button.update(false, 64, 35);
  if (!button.stableLevel())
    return false;
  button.update(false, 65, 35);
  return !button.stableLevel();
}

bool testLedElectricalMapping() {
  return !io_example::ledElectricalLevel(false, false, false) &&
         io_example::ledElectricalLevel(true, false, false) &&
         io_example::ledElectricalLevel(false, false, true) &&
         !io_example::ledElectricalLevel(true, false, true) &&
         !io_example::ledElectricalLevel(true, true, false) &&
         io_example::ledElectricalLevel(true, true, true);
}

bool testAnalogOutputPinPolicy() {
  static constexpr int kOwnedPins[] = {18, 25, 26, 33, 36};
  return io_example::isSafeEsp32PwmPin(32) &&
         !io_example::isSafeEsp32PwmPin(34) &&
         !io_example::isSafeEsp32PwmPin(12) &&
         io_example::isEsp32DacPin(25) && io_example::isEsp32DacPin(26) &&
         !io_example::isEsp32DacPin(32) &&
         io_example::isUnownedOutputPin(32, kOwnedPins, sizeof(kOwnedPins) / sizeof(kOwnedPins[0])) &&
         !io_example::isUnownedOutputPin(25, kOwnedPins, sizeof(kOwnedPins) / sizeof(kOwnedPins[0])) &&
         !io_example::isUnownedOutputPin(32, nullptr, 1);
}

bool testAnalogOutputScalingAndSafeStartup() {
  uint8_t duty = 0;
  return io_example::scaleOutputPercentToPwm(0.0F, 0.0F, 100.0F, false, true, duty) &&
         duty == 0 &&
         io_example::scaleOutputPercentToPwm(100.0F, 0.0F, 100.0F, false, true, duty) &&
         duty == 255 &&
         io_example::scaleOutputPercentToPwm(50.0F, 0.0F, 100.0F, true, true, duty) &&
         duty == 128 &&
         io_example::scaleOutputPercentToPwm(100.0F, 0.0F, 100.0F, false, false, duty) &&
         duty == 0 &&
         !io_example::scaleOutputPercentToPwm(50.0F, 100.0F, 0.0F, false, true, duty) &&
         !io_example::scaleOutputPercentToPwm(NAN, 0.0F, 100.0F, false, true, duty);
}

} // namespace

int main() {
  return testAdcScaling() && testDs18b20FaultMapping() && testButtonDebouncing() &&
             testLedElectricalMapping() && testAnalogOutputPinPolicy() &&
             testAnalogOutputScalingAndSafeStartup()
           ? 0
           : 1;
}
