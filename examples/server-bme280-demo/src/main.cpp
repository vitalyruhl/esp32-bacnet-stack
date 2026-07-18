// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <Arduino.h>
#include <ArduinoBacnetClient.h>
#include <ArduinoBacnetServer.h>
#include <BME280_I2C.h>
#include <ConfigManager.h>
#include <WiFiUdp.h>

#include <cstdio>

#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"
#include "portable/BacnetAnalogValueLimits.h"

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#define BME_DEMO_HAS_SECRETS 1
#else
#include "secret/secrets.example.h"
#define BME_DEMO_HAS_SECRETS 0
#endif

namespace {

constexpr uint32_t kDeviceInstanceDefault = 1682127;
constexpr uint16_t kVendorId = 555; // ASHRAE-reserved local demo value.
constexpr uint32_t kAvBaseDefault = 300;
constexpr uint16_t kBmeAddressDefault = 0x76;
constexpr uint32_t kReadIntervalMs = 1000;
constexpr char kVersion[] = "0.35.0";
constexpr char kAppName[] = "BACnet BME280 Server";

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

  void placeInUi() const {
    ConfigManager.addSettingsPage("BACnet", 20);
    ConfigManager.addSettingsGroup("BACnet", "BACnet", "Startup (restart required)", 20);
    ConfigManager.addToSettingsGroup(deviceInstance->getKey(), "BACnet", "BACnet", "Startup (restart required)", 10);
    ConfigManager.addToSettingsGroup(udpPort->getKey(), "BACnet", "BACnet", "Startup (restart required)", 20);
    ConfigManager.addToSettingsGroup(avBaseInstance->getKey(), "BACnet", "BACnet", "Startup (restart required)", 30);
    ConfigManager.addSettingsPage("Sensor", 30);
    ConfigManager.addSettingsGroup("Sensor", "Sensor", "BME280 and Dew Point", 30);
    ConfigManager.addToSettingsGroup(bmeAddress->getKey(), "Sensor", "Sensor", "BME280 and Dew Point", 10);
    ConfigManager.addToSettingsGroup(warningMinimum->getKey(), "Sensor", "Sensor", "BME280 and Dew Point", 20);
    ConfigManager.addToSettingsGroup(warningMaximum->getKey(), "Sensor", "Sensor", "BME280 and Dew Point", 30);
    ConfigManager.addToSettingsGroup(errorMinimum->getKey(), "Sensor", "Sensor", "BME280 and Dew Point", 40);
    ConfigManager.addToSettingsGroup(errorMaximum->getKey(), "Sensor", "Sensor", "BME280 and Dew Point", 50);
    ConfigManager.addToSettingsGroup(hysteresis->getKey(), "Sensor", "Sensor", "BME280 and Dew Point", 60);
  }
};

Settings settings;
static cm::CoreSettings& coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings& systemSettings = coreSettings.system;
static cm::CoreWiFiSettings& wifiSettings = coreSettings.wifi;
static cm::CoreNtpSettings& ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;
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
bool bacnetConfigured = false;
bool bacnetBound = false;
uint32_t configuredDeviceInstance = 0;
uint32_t configuredAvBase = 0;
uint16_t configuredPort = 0;
uint8_t configuredBmeAddress = 0;
char liveValueLabels[4][32] = {};

void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();
static void setupNetworkDefaults();
static void setupRuntimeUI();

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
  // BME280_I2C exposes pressure in hPa; BACnet publishes this AV in Pascals.
  const float pressure = bme.data.pressure * 100.0F;
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

void configureBacnetAndSensor() {
  validStartupSettings(configuredDeviceInstance, configuredPort, configuredAvBase, configuredBmeAddress);
  values[0] = {configuredAvBase, "AV Temperature", 0.0F, BacnetEngineeringUnits::DegreesCelsius, false, nullptr, nullptr, &metadata[0]};
  values[1] = {configuredAvBase + 1U, "AV Relative Humidity", 0.0F, BacnetEngineeringUnits::PercentRelativeHumidity, false, nullptr, nullptr, &metadata[1]};
  values[2] = {configuredAvBase + 2U, "AV Pressure", 0.0F, BacnetEngineeringUnits::Pascals, false, nullptr, nullptr, &metadata[2]};
  values[3] = {configuredAvBase + 3U, "AV Dew Point", 0.0F, BacnetEngineeringUnits::DegreesCelsius, false, nullptr, nullptr, &metadata[3]};
  bme.setAddress(configuredBmeAddress, 21, 22);
  if (!bme.begin(bme.BME280_STANDBY_0_5, bme.BME280_FILTER_OFF, bme.BME280_SPI3_DISABLE, bme.BME280_OVERSAMPLING_1, bme.BME280_OVERSAMPLING_1, bme.BME280_OVERSAMPLING_1, bme.BME280_MODE_NORMAL)) {
    Serial.println("[E] BME280 initialization failed; AVs report sensor fault");
  }
  bacnetConfigured = bacnetServer.setAnalogValues(values, 4);
  Serial.print("[I] BME280 address 0x");
  Serial.println(configuredBmeAddress, HEX);
  Serial.print("[I] BACnet Device Instance ");
  Serial.println(configuredDeviceInstance);
  Serial.print("[I] BACnet UDP Port ");
  Serial.println(configuredPort);
  Serial.print("[I] BACnet AV range ");
  Serial.print(configuredAvBase);
  Serial.print("-");
  Serial.println(configuredAvBase + 3U);
}

static void setupRuntimeUI() {
  std::snprintf(liveValueLabels[0], sizeof(liveValueLabels[0]), "Temperature (AV%lu)", static_cast<unsigned long>(values[0].instance));
  std::snprintf(liveValueLabels[1], sizeof(liveValueLabels[1]), "Relative Humidity (AV%lu)", static_cast<unsigned long>(values[1].instance));
  std::snprintf(liveValueLabels[2], sizeof(liveValueLabels[2]), "Pressure (AV%lu)", static_cast<unsigned long>(values[2].instance));
  std::snprintf(liveValueLabels[3], sizeof(liveValueLabels[3]), "Dew Point (AV%lu)", static_cast<unsigned long>(values[3].instance));

  auto live = ConfigManager.liveGroup("bme280Bacnet")
                .page("Sensors", 10)
                .card("BME280 BACnet Values");
  live.value("temperature", []() { return values[0].presentValue; })
    .label(liveValueLabels[0])
    .unit("°C")
    .precision(3)
    .order(10);
  live.value("humidity", []() { return values[1].presentValue; })
    .label(liveValueLabels[1])
    .unit("%RH")
    .precision(3)
    .order(20);
  live.value("pressure", []() { return values[2].presentValue; })
    .label(liveValueLabels[2])
    .unit("Pa")
    .precision(1)
    .order(30);
  live.value("dewPoint", []() { return values[3].presentValue; })
    .label(liveValueLabels[3])
    .unit("°C")
    .precision(3)
    .order(40);
}

void startBacnetWhenConnected() {
  if (bacnetBound || !bacnetConfigured)
    return;
  const BacnetServerDevice device{configuredDeviceInstance, kVendorId, "ESP32 BME280 BACnet Server", "Unregistered BACnet Test Server", "ESP32 BME280 Server Demo", kVersion, nullptr};
  bacnetBound = bacnetServer.begin(device, configuredPort);
  if (!bacnetBound)
    Serial.println("[E] BACnet UDP bind failed");
}

} // namespace

void setup() {
  Serial.begin(115200);
  ConfigManager.setAppName(kAppName);
  ConfigManager.setAppTitle(kAppName);
  ConfigManager.setVersion(kVersion);
  ConfigManager.enableBuiltinSystemProvider();
  coreSettings.attachWiFi(ConfigManager);
  coreSettings.attachSystem(ConfigManager);
  coreSettings.attachNtp(ConfigManager);
  settings.create();
  settings.placeInUi();
  ConfigManager.loadAll();
  Serial.println("[I] Persisted settings loaded");
  setupNetworkDefaults();
  configureBacnetAndSensor();
  setupRuntimeUI();
#if defined(WIFI_FILTER_MAC_PRIORITY)
  ConfigManager.setAccessPointMacPriority(WIFI_FILTER_MAC_PRIORITY);
#endif
  ConfigManager.startWebServer();
}

void loop() {
  ConfigManager.getWiFiManager().update();
  ConfigManager.handleClient();
  if (millis() - lastReadMs >= kReadIntervalMs) {
    lastReadMs = millis();
    readSensor();
  }
  if (bacnetBound)
    static_cast<void>(bacnetServer.poll());
}

void onWiFiConnected() {
  wifiServices.onConnected(ConfigManager, kAppName, systemSettings, ntpSettings);
  Serial.print("[I] WiFi connected, local IP ");
  Serial.println(WiFi.localIP());
  startBacnetWhenConnected();
}

void onWiFiDisconnected() {
  wifiServices.onDisconnected();
  if (bacnetBound)
    bacnetServer.end();
  bacnetBound = false;
  Serial.println("[W] WiFi disconnected");
}

void onWiFiAPMode() {
  wifiServices.onAPMode();
  Serial.print("[I] WiFi AP mode IP ");
  Serial.println(WiFi.softAPIP());
}

namespace {

static void setupNetworkDefaults() {
  if (!wifiSettings.wifiSsid.get().isEmpty())
    return;
#if BME_DEMO_HAS_SECRETS
  Serial.println("[I] Applying local WiFi defaults once");
  wifiSettings.wifiSsid.set(MY_WIFI_SSID);
  wifiSettings.wifiPassword.set(MY_WIFI_PASSWORD);
  wifiSettings.staticIp.set(MY_WIFI_IP);
  wifiSettings.useDhcp.set(MY_USE_DHCP);
  wifiSettings.gateway.set(MY_GATEWAY_IP);
  wifiSettings.subnet.set(MY_SUBNET_MASK);
  wifiSettings.dnsPrimary.set(MY_DNS_IP);
  ConfigManager.saveAll();
  Serial.println("[I] Restarting after saving WiFi defaults");
  delay(500);
  ESP.restart();
#else
  Serial.println("[W] WiFi settings empty and secret/secrets.h is missing");
#endif
}

} // namespace
