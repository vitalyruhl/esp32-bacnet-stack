// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetServer.h"

#include <cstdint>
#include <cstring>

namespace {

class TestTransport final : public BacnetDatagramTransport {
public:
  bool begin(uint16_t) override { return true; }
  void end() override {}
  bool send(const BacnetIpEndpoint&, const uint8_t*, size_t) override { return true; }
  size_t receive(uint8_t*, size_t, BacnetIpEndpoint&) override { return 0; }
  void idle() override {}
};

bool testAnalogInputConfiguration() {
  float lightValue = 12.5F;
  BacnetAnalogInput lightSensor;
  if (lightSensor.configure(0, "Light Sensor") != BacnetObjectConfigurationStatus::Ok ||
      lightSensor.bindPresentValue(&lightValue) != BacnetObjectConfigurationStatus::Ok) {
    return false;
  }
  lightSensor.setUnits(BacnetEngineeringUnits::Percent);
  if (lightSensor.addProperty(BacnetPropertyId::Description, "LDR light level") !=
        BacnetObjectConfigurationStatus::Ok ||
      lightSensor.addProperty(BacnetPropertyId::MinPresentValue, 0.0F) !=
        BacnetObjectConfigurationStatus::Ok ||
      lightSensor.addProperty(BacnetPropertyId::MaxPresentValue, 100.0F) !=
        BacnetObjectConfigurationStatus::Ok ||
      lightSensor.addProperty(BacnetPropertyId::Resolution, 0.1F) !=
        BacnetObjectConfigurationStatus::Ok) {
    return false;
  }
  BacnetValue value;
  if (!lightSensor.readOptionalProperty(BacnetPropertyId::Description, value) ||
      value.type != BacnetValueType::CharacterString || value.textLength != 15 ||
      !lightSensor.readOptionalProperty(BacnetPropertyId::MaxPresentValue, value) ||
      value.type != BacnetValueType::Real || value.realValue != 100.0F ||
      lightSensor.presentValueProvider == nullptr ||
      lightSensor.presentValueProvider(lightSensor.presentValueContext) != lightValue) {
    return false;
  }
  TestTransport transport;
  BacnetServer server(transport);
  return server.addObject(lightSensor) == BacnetObjectConfigurationStatus::Ok &&
         server.analogInputCount() == 1;
}

bool testConfigurationErrorsBlockRegistration() {
  float value = 1.0F;
  BacnetAnalogInput wrongType;
  wrongType.configure(3, "Wrong Type");
  wrongType.bindPresentValue(&value);
  if (wrongType.addProperty(BacnetPropertyId::Description, 1.0F) !=
        BacnetObjectConfigurationStatus::PropertyTypeMismatch ||
      wrongType.addProperty(BacnetPropertyId::MinPresentValue, 0.0F) !=
        BacnetObjectConfigurationStatus::Ok) {
    return false;
  }
  const BacnetObjectConfigurationError wrongTypeError = wrongType.configurationError();
  if (wrongType.configurationStatus() != BacnetObjectConfigurationStatus::PropertyTypeMismatch ||
      wrongTypeError.status != BacnetObjectConfigurationStatus::PropertyTypeMismatch ||
      wrongTypeError.property != BacnetPropertyId::Description ||
      wrongTypeError.object.type != static_cast<uint16_t>(BacnetObjectType::AnalogInput) ||
      wrongTypeError.object.instance != 3 || wrongTypeError.objectName == nullptr ||
      std::strcmp(wrongTypeError.objectName, "Wrong Type") != 0 ||
      std::strcmp(bacnetPropertyIdText(wrongTypeError.property), "Description") != 0 ||
      std::strcmp(bacnetObjectConfigurationStatusText(wrongTypeError.status),
                  "property type mismatch") != 0) {
    return false;
  }

  BacnetAnalogInput duplicate;
  duplicate.configure(4, "Duplicate");
  duplicate.bindPresentValue(&value);
  if (duplicate.addProperty(BacnetPropertyId::Description, "first") !=
        BacnetObjectConfigurationStatus::Ok ||
      duplicate.addProperty(BacnetPropertyId::Description, "second") !=
        BacnetObjectConfigurationStatus::DuplicateProperty ||
      duplicate.configurationError().property != BacnetPropertyId::Description) {
    return false;
  }

  BacnetAnalogInput unsupported;
  unsupported.configure(5, "Unsupported");
  unsupported.bindPresentValue(&value);
  if (unsupported.addProperty(BacnetPropertyId::Units, BacnetEnumeratedValue{1}) !=
        BacnetObjectConfigurationStatus::PropertyNotSupported ||
      unsupported.configurationError().property != BacnetPropertyId::Units) {
    return false;
  }

  TestTransport transport;
  BacnetServer server(transport);
  if (server.addObject(wrongType) != BacnetObjectConfigurationStatus::PropertyTypeMismatch ||
      server.analogInputCount() != 0) {
    return false;
  }

  BacnetAnalogInput validAfterFailure;
  validAfterFailure.configure(6, "Valid After Failure");
  validAfterFailure.bindPresentValue(&value);
  validAfterFailure.addProperty(BacnetPropertyId::Description, "valid");
  return server.addObject(validAfterFailure) == BacnetObjectConfigurationStatus::Ok &&
         server.analogInputCount() == 1;
}

bool testIndividualAndArrayRegistration() {
  TestTransport transport;
  BacnetServer server(transport);
  BacnetBinaryInput resetButton;
  BacnetBinaryInput midButton;
  bool resetValue = false;
  bool midValue = true;
  if (resetButton.configure(0, "Reset Button") != BacnetObjectConfigurationStatus::Ok ||
      resetButton.bindPresentValue(&resetValue) != BacnetObjectConfigurationStatus::Ok ||
      midButton.configure(1, "Mid Button") != BacnetObjectConfigurationStatus::Ok ||
      midButton.bindPresentValue(&midValue) != BacnetObjectConfigurationStatus::Ok ||
      server.addObject(resetButton) != BacnetObjectConfigurationStatus::Ok ||
      server.addObject(midButton) != BacnetObjectConfigurationStatus::Ok ||
      server.binaryInputCount() != 2) {
    return false;
  }

  TestTransport arrayTransport;
  BacnetServer arrayServer(arrayTransport);
  BacnetAnalogInput inputs[3];
  float values[3] = {1.0F, 2.0F, 3.0F};
  for (size_t index = 0; index < 3; ++index) {
    if (inputs[index].configure(static_cast<uint32_t>(index), "Array Input") !=
          BacnetObjectConfigurationStatus::Ok ||
        inputs[index].bindPresentValue(&values[index]) !=
          BacnetObjectConfigurationStatus::Ok ||
        arrayServer.addObject(inputs[index]) != BacnetObjectConfigurationStatus::Ok) {
      return false;
    }
  }
  return arrayServer.analogInputCount() == 3;
}

bool testCommandableOutputFacade() {
  BacnetBinaryOutput output;
  if (output.configure(0, "LED 1") != BacnetObjectConfigurationStatus::Ok) {
    return false;
  }
  output.setRelinquishDefault(false);
  if (!output.priority.write(16, false, true) || output.priority.effectivePriority() != 16 ||
      !output.priority.write(8, false, false) || output.priority.effectivePriority() != 8 ||
      output.priority.effectiveValue()) {
    return false;
  }
  return output.priority.write(8, true) && output.priority.effectivePriority() == 16 &&
         output.priority.effectiveValue();
}

} // namespace

int main() {
  return testAnalogInputConfiguration() && testConfigurationErrorsBlockRegistration() &&
           testIndividualAndArrayRegistration() &&
           testCommandableOutputFacade()
           ? 0
           : 1;
}
