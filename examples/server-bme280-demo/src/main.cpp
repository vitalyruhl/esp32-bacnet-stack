// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <Arduino.h>
#include <ArduinoBacnetClient.h>
#include <ArduinoBacnetServer.h>
#include <BME280_I2C.h>
#include <ConfigManager.h>
#include <WiFiUdp.h>

#include "core/CoreSettings.h"
#include "portable/BacnetAnalogValueLimits.h"

namespace {

constexpr uint32_t kDeviceInstanceDefault = 1682127;
constexpr uint16_t kVendorId = 555; // ASHRAE-reserved local demo value.
constexpr uint32_t kAvBaseDefault = 300;
constexpr uint16_t kBmeAddressDefault = 0x76;
constexpr uint32_t kReadIntervalMs = 1000;
constexpr char kVersion[] = "0.35.0";

constexpr BacnetAnalogValueLimitConfig kDewPointDefaults{-40.0F, -5.0F, 25.0F, 40.0F, 0.5F};

struct Settings {
  Config<int>* deviceInstance = nullptr;
  Config<int>* udpPort = nullptr;
  Config<int>* avBaseInstance = nullptr;
  Config<int>* bmeAddress = nullptr;
  Config<float>* warningMinimum = nullptr;
  Config<float>* warningMaximum = nullptr;
  Config<float>* errorMinimum = nullptr;
  Config<float>* errorMaximum = nullptr;
  Config<float>* hysteresis = nullptr;

  void create() {
    deviceInstance = &ConfigManager.addSettingInt("bacnetDevice").name("BACnet Device Instance (restart required)").category("BACnet").defaultValue(kDeviceInstanceDefault).build();
    udpPort = &ConfigManager.addSettingInt("bacnetPort").name("BACnet UDP Port (restart required)").category("BACnet").defaultValue(BacnetServer::kDefaultPort).build();
    avBaseInstance = &ConfigManager.addSettingInt("avBase").name("AV Base Instance (restart required)").category("BACnet").defaultValue(kAvBaseDefault).build();
    bmeAddress = &ConfigManager.addSettingInt("bmeAddress").name("BME280 I2C Address (restart required)").category("Sensor").defaultValue(kBmeAddressDefault).build();
    warningMinimum = &ConfigManager.addSettingFloat("dewWarnMin").name("Dew Point Warning Minimum").category("Dew Point").defaultValue(kDewPointDefaults.warningMinimum).build();
    warningMaximum = &ConfigManager.addSettingFloat("dewWarnMax").name("Dew Point Warning Maximum").category("Dew Point").defaultValue(kDewPointDefaults.warningMaximum).build();
    errorMinimum = &ConfigManager.addSettingFloat("dewErrorMin").name("Dew Point Error Minimum").category("Dew Point").defaultValue(kDewPointDefaults.errorMinimum).build();
    errorMaximum = &ConfigManager.addSettingFloat("dewErrorMax").name("Dew Point Error Maximum").category("Dew Point").defaultValue(kDewPointDefaults.errorMaximum).build();
    hysteresis = &ConfigManager.addSettingFloat("dewDeadband").name("Dew Point Deadband/Hysteresis").category("Dew Point").defaultValue(kDewPointDefaults.hysteresis).build();
  }
};

Settings settings;
WiFiUDP udp;
ArduinoUdpDatagramTransport transport(udp);
BacnetServer bacnetServer(transport);
BME280_I2C bme;

BacnetServerAnalogValueMetadata metadata[] = {
  {"BME280 temperature", -40.0F, 85.0F, 0.1F},
  {"BME280 relative humidity", 0.0F, 100.0F, 0.1F},
  {"BME280 pressure", 30000.0F, 110000.0F, 1.0F},
  {"Calculated Magnus dew point", -60.0F, 60.0F, 0.1F},
};
BacnetServerAnalogValue values[4];
BacnetAnalogValueLimitState dewState = BacnetAnalogValueLimitState::Normal;
uint32_t lastReadMs = 0;

float dewPointCelsius(float temperature, float humidity) {
  if (!isfinite(temperature) || !isfinite(humidity) || humidity <= 0.0F || humidity > 100.0F)
    return NAN;
  constexpr float kA = 17.62F;
  constexpr float kB = 243.12F;
  const float gamma = logf(humidity / 100.0F) + kA * temperature / (kB + temperature);
  return kB * gamma / (kA - gamma);
}

void applyDewPointState(float dewPoint, bool valid) {
  BacnetAnalogValueLimitConfig limits{settings.errorMinimum->get(), settings.warningMinimum->get(), settings.warningMaximum->get(), settings.errorMaximum->get(), settings.hysteresis->get()};
  if (!bacnetAnalogValueLimitConfigIsValid(limits))
    limits = kDewPointDefaults;
  const BacnetAnalogValueLimitResult result = bacnetAnalogValueLimitEvaluate(dewPoint, valid, limits, dewState);
  dewState = result.state;
  metadata[3].eventState = result.eventState;
  metadata[3].reliability = result.reliability;
  metadata[3].inAlarm = result.inAlarm;
  metadata[3].fault = result.fault;
}

void readSensor() {
  bme.read();
  const float temperature = bme.data.temperature;
  const float humidity = bme.data.humidity;
  const float pressure = bme.data.pressure;
  const float dewPoint = dewPointCelsius(temperature, humidity);
  const bool valid = isfinite(temperature) && isfinite(humidity) && isfinite(pressure) && isfinite(dewPoint);
  if (!valid) {
    for (BacnetServerAnalogValueMetadata& item : metadata) {
      item.eventState = 1;
      item.reliability = 1;
      item.inAlarm = false;
      item.fault = true;
    }
    return; // Keep the last valid values on sensor failure.
  }
  values[0].presentValue = temperature;
  values[1].presentValue = humidity;
  values[2].presentValue = pressure;
  values[3].presentValue = dewPoint;
  for (size_t index = 0; index < 3; ++index) {
    metadata[index].eventState = 0;
    metadata[index].reliability = 0;
    metadata[index].inAlarm = false;
    metadata[index].fault = false;
  }
  applyDewPointState(dewPoint, true);
}

bool validStartupSettings(uint32_t& deviceInstance, uint16_t& port, uint32_t& avBase, uint8_t& address) {
  deviceInstance = static_cast<uint32_t>(settings.deviceInstance->get());
  port = static_cast<uint16_t>(settings.udpPort->get());
  avBase = static_cast<uint32_t>(settings.avBaseInstance->get());
  address = static_cast<uint8_t>(settings.bmeAddress->get());
  const bool valid = deviceInstance <= 0x003fffffU && port != 0 && avBase <= 0x003ffffbU && (address == 0x76 || address == 0x77);
  if (!valid) {
    Serial.println("[E] Invalid saved BACnet/BME280 settings; using safe defaults");
    deviceInstance = kDeviceInstanceDefault;
    port = BacnetServer::kDefaultPort;
    avBase = kAvBaseDefault;
    address = kBmeAddressDefault;
  }
  return valid;
}

} // namespace

void setup() {
  Serial.begin(115200);
  ConfigManager.setAppName("BACnet BME280 Server");
  ConfigManager.setAppTitle("BACnet BME280 Server");
  ConfigManager.setVersion(kVersion);
  ConfigManager.enableBuiltinSystemProvider();
  settings.create();
  ConfigManager.loadAll();
  uint32_t deviceInstance = 0;
  uint32_t avBase = 0;
  uint16_t port = 0;
  uint8_t address = 0;
  validStartupSettings(deviceInstance, port, avBase, address);
  const BacnetAnalogValueLimitConfig limits{settings.errorMinimum->get(), settings.warningMinimum->get(), settings.warningMaximum->get(), settings.errorMaximum->get(), settings.hysteresis->get()};
  if (!bacnetAnalogValueLimitConfigIsValid(limits))
    Serial.println("[E] Invalid dew point limits; using safe defaults at runtime");
  values[0] = {avBase + 0U, "AV Temperature", 0.0F, BacnetEngineeringUnits::DegreesCelsius, false, nullptr, nullptr, &metadata[0]};
  values[1] = {avBase + 1U, "AV Relative Humidity", 0.0F, BacnetEngineeringUnits::PercentRelativeHumidity, false, nullptr, nullptr, &metadata[1]};
  values[2] = {avBase + 2U, "AV Pressure", 0.0F, BacnetEngineeringUnits::Pascals, false, nullptr, nullptr, &metadata[2]};
  values[3] = {avBase + 3U, "AV Dew Point", 0.0F, BacnetEngineeringUnits::DegreesCelsius, false, nullptr, nullptr, &metadata[3]};
  bme.setAddress(address, 21, 22);
  if (!bme.begin(bme.BME280_STANDBY_0_5,
                 bme.BME280_FILTER_OFF,
                 bme.BME280_SPI3_DISABLE,
                 bme.BME280_OVERSAMPLING_1,
                 bme.BME280_OVERSAMPLING_1,
                 bme.BME280_OVERSAMPLING_1,
                 bme.BME280_MODE_NORMAL))
    Serial.println("[E] BME280 initialization failed; AVs report sensor fault");
  const BacnetServerDevice device{deviceInstance, kVendorId, "ESP32 BME280 BACnet Server", "Unregistered BACnet Test Server", "ESP32 BME280 Server Demo", kVersion, nullptr};
  if (!bacnetServer.setAnalogValues(values, 4) || !bacnetServer.begin(device, port)) {
    Serial.println("[E] BACnet server startup failed");
    return;
  }
  ConfigManager.startWebServer();
}

void loop() {
  ConfigManager.getWiFiManager().update();
  ConfigManager.handleClient();
  if (millis() - lastReadMs >= kReadIntervalMs) {
    lastReadMs = millis();
    readSensor();
  }
  static_cast<void>(bacnetServer.poll());
}
