// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <Arduino.h>
#include <ArduinoBacnetClient.h>
#include <ArduinoBacnetServer.h>
#include <ConfigManager.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <WiFiUdp.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "IoInputLogic.h"
#include "LedOutputLogic.h"
#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#define IO_DEMO_HAS_SECRETS 1
#else
#include "secret/secrets.example.h"
#define IO_DEMO_HAS_SECRETS 0
#endif

namespace {

constexpr uint32_t kDeviceInstanceDefault = 1682127;
constexpr uint16_t kVendorId = 555;
constexpr char kVersion[] = "0.36.0";
constexpr char kAppName[] = "BACnet I/O Server";
constexpr uint32_t kInputPollMs = 100;
constexpr uint32_t kDsReadIntervalMs = 1000;
constexpr uint8_t kLdrIndex = 0;
constexpr uint8_t kTemperatureIndex = 1;
constexpr uint8_t kResetIndex = 0;
constexpr uint8_t kMidIndex = 1;
constexpr uint8_t kSetIndex = 2;
constexpr int kButtonGpioDefaults[] = {14, 33, 4};
constexpr int kLedGpioDefaults[] = {25, 26};

struct Settings {
  Config<int>* deviceInstance = nullptr;
  Config<int>* udpPort = nullptr;
  Config<bool>* ldrEnabled = nullptr;
  Config<int>* ldrGpio = nullptr;
  Config<int>* ldrRawMinimum = nullptr;
  Config<int>* ldrRawMaximum = nullptr;
  Config<bool>* ldrInverted = nullptr;
  Config<float>* ldrTechnicalMinimum = nullptr;
  Config<float>* ldrTechnicalMaximum = nullptr;
  Config<bool>* dsEnabled = nullptr;
  Config<int>* dsGpio = nullptr;
  Config<bool>* buttonEnabled[3] = {};
  Config<int>* buttonGpio[3] = {};
  Config<bool>* buttonInverted[3] = {};
  Config<int>* buttonDebounceMs[3] = {};
  Config<bool>* ledEnabled[2] = {};
  Config<int>* ledGpio[2] = {};
  Config<bool>* ledInverted[2] = {};
  Config<bool>* ledRelinquishDefault[2] = {};

  void create() {
    deviceInstance = &ConfigManager.addSettingInt("bacnetDevice").name("BACnet Device Instance (restart required)").category("BACnet").defaultValue(kDeviceInstanceDefault).build();
    udpPort = &ConfigManager.addSettingInt("bacnetPort").name("BACnet UDP Port (restart required)").category("BACnet").defaultValue(BacnetServer::kDefaultPort).build();
    ldrEnabled = &ConfigManager.addSettingBool("ldrEnabled").name("LDR Enabled (restart required)").category("LDR").defaultValue(true).build();
    ldrGpio = &ConfigManager.addSettingInt("ldrGpio").name("LDR GPIO (restart required)").category("LDR").defaultValue(36).build();
    ldrRawMinimum = &ConfigManager.addSettingInt("ldrRawMin").name("LDR ADC Raw Minimum").category("LDR").defaultValue(0).build();
    ldrRawMaximum = &ConfigManager.addSettingInt("ldrRawMax").name("LDR ADC Raw Maximum").category("LDR").defaultValue(4095).build();
    ldrInverted = &ConfigManager.addSettingBool("ldrInvert").name("LDR Invert").category("LDR").defaultValue(false).build();
    ldrTechnicalMinimum = &ConfigManager.addSettingFloat("ldrTechMin").name("LDR Technical Minimum (%)").category("LDR").defaultValue(0.0F).build();
    ldrTechnicalMaximum = &ConfigManager.addSettingFloat("ldrTechMax").name("LDR Technical Maximum (%)").category("LDR").defaultValue(100.0F).build();
    dsEnabled = &ConfigManager.addSettingBool("dsEnabled").name("DS18B20 Enabled (restart required)").category("DS18B20").defaultValue(true).build();
    dsGpio = &ConfigManager.addSettingInt("dsGpio").name("DS18B20 GPIO (restart required)").category("DS18B20").defaultValue(18).build();
    const char* keys[] = {"reset", "mid", "set"};
    for (size_t index = 0; index < 3; ++index) {
      char key[24] = {};
      std::snprintf(key, sizeof(key), "%sEnabled", keys[index]);
      buttonEnabled[index] = &ConfigManager.addSettingBool(key).name("Button Enabled (restart required)").category("Buttons").defaultValue(true).build();
      std::snprintf(key, sizeof(key), "%sGpio", keys[index]);
      buttonGpio[index] = &ConfigManager.addSettingInt(key).name("Button GPIO (restart required)").category("Buttons").defaultValue(kButtonGpioDefaults[index]).build();
      std::snprintf(key, sizeof(key), "%sInvert", keys[index]);
      buttonInverted[index] = &ConfigManager.addSettingBool(key).name("Button Invert").category("Buttons").defaultValue(false).build();
      std::snprintf(key, sizeof(key), "%sDebounce", keys[index]);
      buttonDebounceMs[index] = &ConfigManager.addSettingInt(key).name("Button Debounce (ms)").category("Buttons").defaultValue(35).build();
    }
    const char* ledKeys[] = {"led1", "led2"};
    for (size_t index = 0; index < 2; ++index) {
      char key[24] = {};
      std::snprintf(key, sizeof(key), "%sEnabled", ledKeys[index]);
      ledEnabled[index] = &ConfigManager.addSettingBool(key).name("LED Enabled (restart required)").category("LED Outputs").defaultValue(true).build();
      std::snprintf(key, sizeof(key), "%sGpio", ledKeys[index]);
      ledGpio[index] = &ConfigManager.addSettingInt(key).name("LED GPIO (restart required)").category("LED Outputs").defaultValue(kLedGpioDefaults[index]).build();
      std::snprintf(key, sizeof(key), "%sInvert", ledKeys[index]);
      ledInverted[index] = &ConfigManager.addSettingBool(key).name("LED Active Low").category("LED Outputs").defaultValue(false).build();
      std::snprintf(key, sizeof(key), "%sRelinquish", ledKeys[index]);
      ledRelinquishDefault[index] = &ConfigManager.addSettingBool(key).name("LED Relinquish Default").category("LED Outputs").defaultValue(false).build();
    }
  }
};

struct RuntimeState {
  io_example::InputHealth health;
};

struct LedBinding {
  int gpio = -1;
  bool configured = false;
  bool inverted = false;
};

Settings settings;
RuntimeState analogState[2];
RuntimeState buttonState[3];
io_example::DebouncedButton buttons[3];
BacnetServerAnalogInput analogInputs[] = {
  {0, "Light Sensor", 0.0F, BacnetEngineeringUnits::Percent},
  {1, "Temperature", 0.0F, BacnetEngineeringUnits::DegreesCelsius},
};
BacnetServerBinaryInput binaryInputs[] = {
  {0, "Reset Button"},
  {1, "Mid Button"},
  {2, "Set Button"},
};
LedBinding ledBindings[2];
BacnetServerBinaryOutput binaryOutputs[] = {
  {0, "LED 1"},
  {1, "LED 2"},
};
const char* const kDescriptions[] = {
  "LDR voltage divider with bias potentiometer",
  "DS18B20 temperature sensor",
  "Low-active reset button",
  "Low-active mid button",
  "Low-active set button",
  "GPIO25 development-station LED",
  "GPIO26 development-station LED",
};
float analogMinimum[] = {0.0F, -55.0F};
float analogMaximum[] = {100.0F, 125.0F};
float analogResolution[] = {0.1F, 0.0625F};
BacnetServerPropertyRegistration properties[30] = {};
WiFiUDP udp;
ArduinoUdpDatagramTransport transport(udp);
BacnetServer bacnetServer(transport);
OneWire oneWire(18);
DallasTemperature ds18b20(&oneWire);
cm::CoreSettings& coreSettings = cm::CoreSettings::instance();
cm::CoreWiFiServices wifiServices;
bool ldrConfigured = false;
bool dsConfigured = false;
bool buttonConfigured[3] = {};
bool ledConfigured[2] = {};
bool bacnetConfigured = false;
bool bacnetBound = false;
uint32_t deviceInstance = kDeviceInstanceDefault;
uint16_t udpPort = BacnetServer::kDefaultPort;
int ldrRaw = 0;
uint32_t lastInputPollMs = 0;
uint32_t lastDsReadMs = 0;

bool readText(const void* context, BacnetValue& value) {
  const char* text = static_cast<const char*>(context);
  if (text == nullptr || std::strlen(text) >= sizeof(value.text))
    return false;
  value = BacnetValue{};
  value.type = BacnetValueType::CharacterString;
  value.textLength = std::strlen(text);
  std::memcpy(value.text, text, value.textLength + 1U);
  return true;
}
bool readFloat(const void* context, BacnetValue& value) {
  const auto* number = static_cast<const float*>(context);
  if (number == nullptr || !std::isfinite(*number))
    return false;
  value = BacnetValue{};
  value.type = BacnetValueType::Real;
  value.realValue = *number;
  return true;
}
bool readReliability(const void* context, BacnetValue& value) {
  const auto* state = static_cast<const RuntimeState*>(context);
  if (state == nullptr)
    return false;
  value = BacnetValue{};
  value.type = BacnetValueType::Enumerated;
  value.unsignedValue = state->health.reliability;
  return true;
}
bool readEventState(const void* context, BacnetValue& value) {
  const auto* state = static_cast<const RuntimeState*>(context);
  if (state == nullptr)
    return false;
  value = BacnetValue{};
  value.type = BacnetValueType::Enumerated;
  value.unsignedValue = state->health.eventState;
  return true;
}
bool readStatusFlags(const void* context, BacnetValue& value) {
  const auto* state = static_cast<const RuntimeState*>(context);
  if (state == nullptr)
    return false;
  value = BacnetValue{};
  value.type = BacnetValueType::BitString;
  value.bitStringValue = (state->health.fault ? 1UL << 1U : 0UL) |
                         (state->health.outOfService ? 1UL << 3U : 0UL);
  value.bitStringBitCount = 4;
  return true;
}

bool isUsableInputGpio(int gpio) {
  return gpio >= 0 && gpio <= 39 && !(gpio >= 6 && gpio <= 11);
}
bool isAdc1Gpio(int gpio) {
  return gpio >= 32 && gpio <= 39;
}
bool isUsableOutputGpio(int gpio) {
  return gpio >= 0 && gpio <= 33 && !(gpio >= 6 && gpio <= 11);
}
bool conflictsKnownReservedGpio(int gpio, int designatedGpio) {
  return gpio != designatedGpio && (gpio == 12 || gpio == 18 || gpio == 21 || gpio == 22 ||
                                    gpio == 23 || gpio == 25 || gpio == 26 || gpio == 27 ||
                                    gpio == 36 || gpio == 39);
}
bool uniqueEnabledPin(int gpio, const int* pins, const bool* enabled, size_t count, size_t self) {
  for (size_t index = 0; index < count; ++index)
    if (index != self && enabled[index] && pins[index] == gpio)
      return false;
  return true;
}

bool ledConflictsKnownReservedGpio(int gpio, int designatedGpio) {
  return gpio != designatedGpio && (gpio == 4 || gpio == 12 || gpio == 14 || gpio == 18 ||
                                    gpio == 19 || gpio == 21 || gpio == 22 || gpio == 23 ||
                                    gpio == 25 || gpio == 26 || gpio == 27 || gpio == 33 ||
                                    gpio == 36 || gpio == 39);
}

void applyLedOutput(void* context, bool presentValue, bool outOfService) {
  auto* binding = static_cast<LedBinding*>(context);
  if (binding == nullptr || !binding->configured)
    return;
  digitalWrite(binding->gpio,
               io_example::ledElectricalLevel(presentValue, outOfService, binding->inverted) ? HIGH : LOW);
}

void configureInputs() {
  const int pins[] = {settings.ldrGpio->get(), settings.dsGpio->get(), settings.buttonGpio[0]->get(), settings.buttonGpio[1]->get(), settings.buttonGpio[2]->get()};
  const bool enabled[] = {settings.ldrEnabled->get(), settings.dsEnabled->get(), settings.buttonEnabled[0]->get(), settings.buttonEnabled[1]->get(), settings.buttonEnabled[2]->get()};
  ldrConfigured = enabled[0] && isAdc1Gpio(pins[0]) && !conflictsKnownReservedGpio(pins[0], 36) && uniqueEnabledPin(pins[0], pins, enabled, 5, 0) && settings.ldrRawMinimum->get() != settings.ldrRawMaximum->get() && std::isfinite(settings.ldrTechnicalMinimum->get()) && std::isfinite(settings.ldrTechnicalMaximum->get()) && settings.ldrTechnicalMinimum->get() <= settings.ldrTechnicalMaximum->get();
  dsConfigured = enabled[1] && isUsableInputGpio(pins[1]) && !conflictsKnownReservedGpio(pins[1], 18) && uniqueEnabledPin(pins[1], pins, enabled, 5, 1);
  for (size_t index = 0; index < 3; ++index) {
    const size_t pinIndex = index + 2;
    buttonConfigured[index] = enabled[pinIndex] && isUsableInputGpio(pins[pinIndex]) && !conflictsKnownReservedGpio(pins[pinIndex], kButtonGpioDefaults[index]) && uniqueEnabledPin(pins[pinIndex], pins, enabled, 5, pinIndex) && settings.buttonDebounceMs[index]->get() >= 0;
    if (buttonConfigured[index]) {
      pinMode(pins[pinIndex], INPUT_PULLUP);
      buttons[index].begin(digitalRead(pins[pinIndex]) != LOW, millis());
    }
    buttonState[index].health = io_example::inputHealth(enabled[pinIndex], buttonConfigured[index]);
    binaryInputs[index].outOfService = !enabled[pinIndex];
  }
  analogState[kLdrIndex].health = io_example::inputHealth(enabled[0], ldrConfigured);
  analogState[kTemperatureIndex].health = io_example::inputHealth(enabled[1], dsConfigured);
  analogInputs[kLdrIndex].outOfService = !enabled[0];
  analogInputs[kTemperatureIndex].outOfService = !enabled[1];
  if (ldrConfigured)
    analogReadResolution(12);
  if (dsConfigured) {
    oneWire = OneWire(pins[1]);
    ds18b20.setOneWire(&oneWire);
    ds18b20.begin();
    ds18b20.setWaitForConversion(false);
    ds18b20.requestTemperatures();
  }
}

void configureOutputs() {
  const int inputPins[] = {settings.ldrGpio->get(), settings.dsGpio->get(), settings.buttonGpio[0]->get(), settings.buttonGpio[1]->get(), settings.buttonGpio[2]->get()};
  const bool inputEnabled[] = {settings.ldrEnabled->get(), settings.dsEnabled->get(), settings.buttonEnabled[0]->get(), settings.buttonEnabled[1]->get(), settings.buttonEnabled[2]->get()};
  const int outputPins[] = {settings.ledGpio[0]->get(), settings.ledGpio[1]->get()};
  const bool outputEnabled[] = {settings.ledEnabled[0]->get(), settings.ledEnabled[1]->get()};
  for (size_t index = 0; index < 2; ++index) {
    bool conflictsInput = false;
    for (size_t input = 0; input < 5; ++input)
      conflictsInput = conflictsInput || (inputEnabled[input] && outputPins[index] == inputPins[input]);
    ledConfigured[index] = outputEnabled[index] && isUsableOutputGpio(outputPins[index]) &&
                           !ledConflictsKnownReservedGpio(outputPins[index], kLedGpioDefaults[index]) && !conflictsInput &&
                           uniqueEnabledPin(outputPins[index], outputPins, outputEnabled, 2, index);
    ledBindings[index] = {outputPins[index], ledConfigured[index], settings.ledInverted[index]->get()};
    binaryOutputs[index].priority.relinquishDefault = settings.ledRelinquishDefault[index]->get();
    binaryOutputs[index].outOfService = !ledConfigured[index];
    binaryOutputs[index].polarity = settings.ledInverted[index]->get() ? 1U : 0U;
    binaryOutputs[index].reliability = ledConfigured[index] ? 0U : 10U;
    binaryOutputs[index].apply = applyLedOutput;
    binaryOutputs[index].applyContext = &ledBindings[index];
    if (ledConfigured[index]) {
      const bool inactiveLevel = io_example::ledElectricalLevel(
        false, false, settings.ledInverted[index]->get());
      digitalWrite(outputPins[index], inactiveLevel ? HIGH : LOW);
      pinMode(outputPins[index], OUTPUT);
    }
  }
}

void registerProperties() {
  size_t index = 0;
  for (size_t input = 0; input < 2; ++input) {
    const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::AnalogInput), analogInputs[input].instance};
    properties[index++] = {object, BacnetPropertyId::Description, readText, kDescriptions[input]};
    properties[index++] = {object, BacnetPropertyId::MinPresentValue, readFloat, &analogMinimum[input]};
    properties[index++] = {object, BacnetPropertyId::MaxPresentValue, readFloat, &analogMaximum[input]};
    properties[index++] = {object, BacnetPropertyId::Resolution, readFloat, &analogResolution[input]};
    properties[index++] = {object, BacnetPropertyId::StatusFlags, readStatusFlags, &analogState[input]};
    properties[index++] = {object, BacnetPropertyId::EventState, readEventState, &analogState[input]};
    properties[index++] = {object, BacnetPropertyId::Reliability, readReliability, &analogState[input]};
  }
  for (size_t input = 0; input < 3; ++input) {
    const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::BinaryInput), binaryInputs[input].instance};
    properties[index++] = {object, BacnetPropertyId::Description, readText, kDescriptions[input + 2]};
    properties[index++] = {object, BacnetPropertyId::StatusFlags, readStatusFlags, &buttonState[input]};
    properties[index++] = {object, BacnetPropertyId::EventState, readEventState, &buttonState[input]};
    properties[index++] = {object, BacnetPropertyId::Reliability, readReliability, &buttonState[input]};
  }
  for (size_t output = 0; output < 2; ++output) {
    const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::BinaryOutput), binaryOutputs[output].instance};
    properties[index++] = {object, BacnetPropertyId::Description, readText, kDescriptions[output + 5]};
  }
  bacnetConfigured = bacnetServer.setAnalogInputs(analogInputs, 2) && bacnetServer.setBinaryInputs(binaryInputs, 3) && bacnetServer.setBinaryOutputs(binaryOutputs, 2) && bacnetServer.setPropertyRegistrations(properties, index);
}

void pollInputs(uint32_t now) {
  if (now - lastInputPollMs < kInputPollMs)
    return;
  lastInputPollMs = now;
  if (ldrConfigured) {
    ldrRaw = analogRead(settings.ldrGpio->get());
    float scaled = 0.0F;
    const bool valid = io_example::scaleAdcPercent(ldrRaw, settings.ldrRawMinimum->get(), settings.ldrRawMaximum->get(), settings.ldrInverted->get(), settings.ldrTechnicalMinimum->get(), settings.ldrTechnicalMaximum->get(), scaled);
    analogState[kLdrIndex].health = io_example::inputHealth(true, valid);
    if (valid)
      analogInputs[kLdrIndex].presentValue = scaled;
  }
  for (size_t index = 0; index < 3; ++index)
    if (buttonConfigured[index]) {
      const bool rawHigh = digitalRead(settings.buttonGpio[index]->get()) == HIGH;
      buttons[index].update(rawHigh, now, static_cast<uint32_t>(settings.buttonDebounceMs[index]->get()));
      const bool pressed = settings.buttonInverted[index]->get() ? buttons[index].stableLevel() : !buttons[index].stableLevel();
      binaryInputs[index].presentValue = pressed;
    }
  if (dsConfigured && now - lastDsReadMs >= kDsReadIntervalMs) {
    lastDsReadMs = now;
    const float temperature = ds18b20.getTempCByIndex(0);
    const bool valid = std::isfinite(temperature) && temperature != DEVICE_DISCONNECTED_C && temperature >= -55.0F && temperature <= 125.0F;
    analogState[kTemperatureIndex].health = io_example::inputHealth(true, valid);
    if (valid)
      analogInputs[kTemperatureIndex].presentValue = temperature;
    ds18b20.requestTemperatures();
  }
}

void startBacnetWhenConnected() {
  if (bacnetBound || !bacnetConfigured)
    return;
  const BacnetServerDevice device{deviceInstance, kVendorId, "ESP32 I/O BACnet Server", "Unregistered BACnet Test Server", "ESP32 Read-Only I/O Server", kVersion, nullptr};
  bacnetBound = bacnetServer.begin(device, udpPort);
  if (!bacnetBound)
    Serial.println("[E] BACnet UDP bind failed");
  else
    Serial.println("[I] BACnet server online");
}

void setupNetworkDefaults() {
  if (!coreSettings.wifi.wifiSsid.get().isEmpty())
    return;
#if IO_DEMO_HAS_SECRETS
  coreSettings.wifi.wifiSsid.set(MY_WIFI_SSID);
  coreSettings.wifi.wifiPassword.set(MY_WIFI_PASSWORD);
  coreSettings.wifi.staticIp.set(MY_WIFI_IP);
  coreSettings.wifi.useDhcp.set(MY_USE_DHCP);
  coreSettings.wifi.gateway.set(MY_GATEWAY_IP);
  coreSettings.wifi.subnet.set(MY_SUBNET_MASK);
  coreSettings.wifi.dnsPrimary.set(MY_DNS_IP);
  ConfigManager.saveAll();
  ESP.restart();
#else
  Serial.println("[W] WiFi settings empty and secret/secrets.h is missing");
#endif
}

} // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("[I] Starting BACnet I/O server");
  ConfigManager.setAppName(kAppName);
  ConfigManager.setAppTitle(kAppName);
  ConfigManager.setVersion(kVersion);
  ConfigManager.enableBuiltinSystemProvider();
  coreSettings.attachWiFi(ConfigManager);
  coreSettings.attachSystem(ConfigManager);
  coreSettings.attachNtp(ConfigManager);
  settings.create();
  ConfigManager.loadAll();
  deviceInstance = static_cast<uint32_t>(settings.deviceInstance->get());
  udpPort = static_cast<uint16_t>(settings.udpPort->get());
  if (deviceInstance > 0x003FFFFFU || udpPort == 0) {
    Serial.println("[E] Invalid BACnet startup settings; using defaults");
    deviceInstance = kDeviceInstanceDefault;
    udpPort = BacnetServer::kDefaultPort;
  }
  setupNetworkDefaults();
  configureInputs();
  configureOutputs();
  registerProperties();
  ConfigManager.startWebServer();
}

void loop() {
  const uint32_t now = millis();
  ConfigManager.getWiFiManager().update();
  ConfigManager.handleClient();
  pollInputs(now);
  if (bacnetBound)
    static_cast<void>(bacnetServer.poll());
}
void onWiFiConnected() {
  wifiServices.onConnected(ConfigManager, kAppName, coreSettings.system, coreSettings.ntp);
  Serial.print("[I] WiFi connected, local IP ");
  Serial.println(WiFi.localIP());
  startBacnetWhenConnected();
}
void onWiFiDisconnected() {
  wifiServices.onDisconnected();
  if (bacnetBound)
    bacnetServer.end();
  bacnetBound = false;
}
void onWiFiAPMode() {
  wifiServices.onAPMode();
}
