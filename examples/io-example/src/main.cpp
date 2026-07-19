// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <Arduino.h>
#include <ArduinoBacnetClient.h>
#include <ConfigManager.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include <cmath>
#include <cstdint>
#include <cstdio>

#include <ArduinoBacnetServer.h>
#include <BacnetServer.h>
#include <core/CoreSettings.h>
#include <core/CoreWiFiServices.h>
#include <io/IOManager.h>

#include "IoInputLogic.h"

namespace {

constexpr char kAppName[] = "ESP32 BACnet I/O Server";
constexpr char kVersion[] = "0.36.0";
constexpr uint16_t kDevelopmentVendorId = 0;
constexpr uint32_t kDeviceInstanceDefault = 1682127;
constexpr int kDs18b20DefaultGpio = 18;
constexpr uint32_t kInputPollMs = 250;
constexpr uint32_t kDsReadIntervalMs = 1000;
constexpr uint32_t kRecentActivityMs = 60000;

struct Settings {
  Config<int>* deviceInstance = nullptr;
  Config<int>* udpPort = nullptr;
  Config<int>* vendorId = nullptr;
  Config<bool>* dsEnabled = nullptr;
  Config<int>* dsGpio = nullptr;

  void create() {
    deviceInstance = &ConfigManager.addSettingInt("bacnetDevice")
                        .name("BACnet Device Instance (restart required)")
                        .category("BACnet")
                        .defaultValue(kDeviceInstanceDefault)
                        .build();
    udpPort = &ConfigManager.addSettingInt("bacnetPort")
                 .name("BACnet UDP Port (restart required)")
                 .category("BACnet")
                 .defaultValue(BacnetServer::kDefaultPort)
                 .build();
    vendorId = &ConfigManager.addSettingInt("bacnetVendorId")
                  .name("BACnet Vendor ID (restart required)")
                  .category("BACnet")
                  .defaultValue(kDevelopmentVendorId)
                  .build();
    dsEnabled = &ConfigManager.addSettingBool("dsEnabled")
                   .name("DS18B20 Enabled (restart required)")
                   .category("DS18B20")
                   .defaultValue(true)
                   .build();
    dsGpio = &ConfigManager.addSettingInt("dsGpio")
                .name("DS18B20 GPIO (restart required)")
                .category("DS18B20")
                .defaultValue(kDs18b20DefaultGpio)
                .build();
  }
};

struct ObjectActivity {
  uint32_t reads = 0;
  uint32_t lastReadMs = 0;
  uint32_t writes = 0;
  uint32_t lastWriteMs = 0;
  uint8_t lastWritePriority = 0;
  BacnetPropertyId lastProperty = BacnetPropertyId::ObjectName;
};

struct BacnetActivityState {
  bool seen = false;
  uint32_t lastSeenMs = 0;
  BacnetIpEndpoint peer;
  BacnetServerActivityService service = BacnetServerActivityService::ReadProperty;
  BacnetObjectId object;
  BacnetPropertyId property = BacnetPropertyId::ObjectName;
  uint8_t priority = 0;
  char peerText[32] = "No recent BACnet activity";
};

struct CovLiveState {
  char peer[32] = "No active subscriptions";
  char object[32] = "None";
  char property[32] = "None";
  char mode[24] = "None";
  char state[24] = "Inactive";
  uint32_t processId = 0;
  uint32_t lifetimeSeconds = 0;
  uint32_t lastSentMs = 0;
  uint32_t lastAckMs = 0;
  size_t count = 0;
};

Settings settings;
cm::IOManager ioManager;
cm::CoreSettings& coreSettings = cm::CoreSettings::instance();
cm::CoreWiFiServices wifiServices;
WiFiUDP udp;
ArduinoUdpDatagramTransport transport(udp);
BacnetServer bacnetServer(transport);
ArduinoMonotonicClock bacnetClock;
OneWire oneWire(kDs18b20DefaultGpio);
DallasTemperature ds18b20(&oneWire);

float lightValue = 0.0F;
float temperatureValue = 0.0F;
bool resetButtonValue = false;
bool midButtonValue = false;
bool setButtonValue = false;
int lightRawValue = 0;

BacnetServerStatusFlags lightStatusFlags;
BacnetServerStatusFlags temperatureStatusFlags;
BacnetServerStatusFlags resetButtonStatusFlags;
BacnetServerStatusFlags midButtonStatusFlags;
BacnetServerStatusFlags setButtonStatusFlags;
BacnetEnumeratedValue lightEventState;
BacnetEnumeratedValue temperatureEventState;
BacnetEnumeratedValue resetButtonEventState;
BacnetEnumeratedValue midButtonEventState;
BacnetEnumeratedValue setButtonEventState;
BacnetEnumeratedValue lightReliability;
BacnetEnumeratedValue temperatureReliability;
BacnetEnumeratedValue resetButtonReliability;
BacnetEnumeratedValue midButtonReliability;
BacnetEnumeratedValue setButtonReliability;

BacnetAnalogInput lightSensor;
BacnetAnalogInput temperatureSensor;
BacnetBinaryInput resetButton;
BacnetBinaryInput midButton;
BacnetBinaryInput setButton;
BacnetBinaryOutput led1;
BacnetBinaryOutput led2;

ObjectActivity lightActivity;
ObjectActivity temperatureActivity;
ObjectActivity resetButtonActivity;
ObjectActivity midButtonActivity;
ObjectActivity setButtonActivity;
ObjectActivity led1Activity;
ObjectActivity led2Activity;
BacnetActivityState bacnetActivity;
CovLiveState covLive;

bool dsConfigured = false;
bool bacnetConfigured = false;
bool bacnetBound = false;
uint32_t deviceInstance = kDeviceInstanceDefault;
uint16_t udpPort = BacnetServer::kDefaultPort;
uint16_t vendorId = kDevelopmentVendorId;
uint32_t lastInputPollMs = 0;
uint32_t lastDsReadMs = 0;

template <typename TObject>
void logObjectError(const TObject& object, BacnetObjectConfigurationStatus registrationStatus) {
  bacnetConfigured = false;
  const BacnetObjectConfigurationError error = object.configurationError();
  const BacnetObjectConfigurationStatus status =
    error.status == BacnetObjectConfigurationStatus::Ok ? registrationStatus : error.status;
  Serial.printf("[E] BACnet object configuration failed: %s (%u:%lu), %s: %s\n",
                error.objectName == nullptr ? "unnamed object" : error.objectName,
                static_cast<unsigned int>(error.object.type),
                static_cast<unsigned long>(error.object.instance),
                bacnetPropertyIdText(error.property),
                bacnetObjectConfigurationStatusText(status));
}

void updateHealth(const io_example::InputHealth& health,
                  BacnetServerStatusFlags& statusFlags,
                  BacnetEnumeratedValue& eventState,
                  BacnetEnumeratedValue& reliability) {
  statusFlags.value = (health.fault ? 1UL << 1U : 0UL) |
                      (health.outOfService ? 1UL << 3U : 0UL);
  statusFlags.bitCount = 4;
  eventState.value = health.eventState;
  reliability.value = health.reliability;
}

const char* reliabilityText(const BacnetEnumeratedValue& reliability) {
  return reliability.value == io_example::kReliabilityNoFaultDetected
           ? "No fault detected"
           : "No sensor / invalid I/O";
}

const char* propertyText(BacnetPropertyId property) {
  switch (property) {
    case BacnetPropertyId::PresentValue:
      return "Present_Value";
    case BacnetPropertyId::Description:
      return "Description";
    case BacnetPropertyId::PropertyList:
      return "Property_List";
    case BacnetPropertyId::PriorityArray:
      return "Priority_Array";
    case BacnetPropertyId::StatusFlags:
      return "Status_Flags";
    case BacnetPropertyId::Reliability:
      return "Reliability";
    case BacnetPropertyId::OutOfService:
      return "Out_Of_Service";
    default:
      return "Other property";
  }
}

const char* covStateText(BacnetServerCovSubscriptionState state) {
  switch (state) {
    case BacnetServerCovSubscriptionState::Active:
      return "Active";
    case BacnetServerCovSubscriptionState::AwaitingConfirmedAck:
      return "Pending ACK";
    default:
      return "Inactive";
  }
}

const char* objectText(BacnetObjectId object);

void refreshCovLiveState() {
  covLive.count = bacnetServer.covSubscriptionCount();
  if (covLive.count == 0) {
    std::snprintf(covLive.peer, sizeof(covLive.peer), "%s", "No active subscriptions");
    std::snprintf(covLive.object, sizeof(covLive.object), "%s", "None");
    std::snprintf(covLive.property, sizeof(covLive.property), "%s", "None");
    std::snprintf(covLive.mode, sizeof(covLive.mode), "%s", "None");
    std::snprintf(covLive.state, sizeof(covLive.state), "%s", "Inactive");
    covLive.processId = 0;
    covLive.lifetimeSeconds = 0;
    covLive.lastSentMs = 0;
    covLive.lastAckMs = 0;
    return;
  }
  BacnetServerCovSubscription subscription;
  if (!bacnetServer.covSubscriptionAt(0, subscription)) {
    return;
  }
  std::snprintf(covLive.peer, sizeof(covLive.peer), "%u.%u.%u.%u:%u", subscription.peer.address[0], subscription.peer.address[1], subscription.peer.address[2], subscription.peer.address[3], subscription.peer.port);
  std::snprintf(covLive.object, sizeof(covLive.object), "%s", objectText(subscription.object));
  std::snprintf(covLive.property, sizeof(covLive.property), "%s", subscription.isPropertySubscription ? propertyText(subscription.property) : "Object (PV + Status_Flags)");
  std::snprintf(covLive.mode, sizeof(covLive.mode), "%s %s", subscription.confirmed ? "Confirmed" : "Unconfirmed", subscription.isPropertySubscription ? "property" : "object");
  std::snprintf(covLive.state, sizeof(covLive.state), "%s", covStateText(subscription.state));
  covLive.processId = subscription.processId;
  const int32_t remainingLifetimeMs =
    static_cast<int32_t>(subscription.expiresAtMs - millis());
  covLive.lifetimeSeconds = subscription.lifetimeSeconds == 0U ||
                                subscription.expiresAtMs == 0U
                              ? 0U
                              : (remainingLifetimeMs <= 0 ? 0U
                                                          : static_cast<uint32_t>(
                                                              (remainingLifetimeMs + 999) / 1000));
  covLive.lastSentMs = subscription.lastSentMs;
  covLive.lastAckMs = subscription.lastAckMs;
}

const char* objectText(BacnetObjectId object) {
  if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogInput)) {
    return object.instance == 0 ? "AI0 Light Sensor" : "AI1 Temperature";
  }
  if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryInput)) {
    if (object.instance == 0)
      return "BI0 Reset Button";
    if (object.instance == 1)
      return "BI1 Mid Button";
    return "BI2 Set Button";
  }
  if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryOutput)) {
    return object.instance == 0 ? "BO0 LED 1" : "BO1 LED 2";
  }
  return "Other BACnet object";
}

ObjectActivity* activityFor(BacnetObjectId object) {
  if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogInput)) {
    return object.instance == 0 ? &lightActivity : (object.instance == 1 ? &temperatureActivity : nullptr);
  }
  if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryInput)) {
    if (object.instance == 0)
      return &resetButtonActivity;
    if (object.instance == 1)
      return &midButtonActivity;
    if (object.instance == 2)
      return &setButtonActivity;
  }
  if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryOutput)) {
    return object.instance == 0 ? &led1Activity : (object.instance == 1 ? &led2Activity : nullptr);
  }
  return nullptr;
}

void observeBacnetActivity(void*, const BacnetServerActivity& activity) {
  const uint32_t now = millis();
  bacnetActivity.seen = true;
  bacnetActivity.lastSeenMs = now;
  bacnetActivity.peer = activity.peer;
  bacnetActivity.service = activity.service;
  bacnetActivity.object = activity.object;
  bacnetActivity.property = activity.property;
  bacnetActivity.priority = activity.hasPriority ? activity.priority : 16U;
  std::snprintf(bacnetActivity.peerText, sizeof(bacnetActivity.peerText), "%u.%u.%u.%u:%u", activity.peer.address[0], activity.peer.address[1], activity.peer.address[2], activity.peer.address[3], activity.peer.port);
  ObjectActivity* objectActivity = activityFor(activity.object);
  if (objectActivity == nullptr) {
    return;
  }
  objectActivity->lastProperty = activity.property;
  if (activity.service == BacnetServerActivityService::ReadProperty) {
    ++objectActivity->reads;
    objectActivity->lastReadMs = now;
  } else {
    ++objectActivity->writes;
    objectActivity->lastWriteMs = now;
    objectActivity->lastWritePriority = activity.hasPriority ? activity.priority : 16U;
  }
}

void applyLed1(void*, bool presentValue, bool outOfService) {
  ioManager.set("led1", !outOfService && presentValue);
}

void applyLed2(void*, bool presentValue, bool outOfService) {
  ioManager.set("led2", !outOfService && presentValue);
}

void registerLightSensor() {
  lightSensor.configure(0, "Light Sensor");
  lightSensor.bindPresentValue(&lightValue);
  lightSensor.setUnits(BacnetEngineeringUnits::Percent);
  lightSensor.addProperty(BacnetPropertyId::Description, "LDR light level");
  lightSensor.addProperty(BacnetPropertyId::MinPresentValue, 0.0F);
  lightSensor.addProperty(BacnetPropertyId::MaxPresentValue, 100.0F);
  lightSensor.addProperty(BacnetPropertyId::Resolution, 0.1F);
  lightSensor.addProperty(BacnetPropertyId::StatusFlags, &lightStatusFlags);
  lightSensor.addProperty(BacnetPropertyId::EventState, &lightEventState);
  lightSensor.addProperty(BacnetPropertyId::Reliability, &lightReliability);
  const auto status = bacnetServer.addObject(lightSensor);
  if (status != BacnetObjectConfigurationStatus::Ok) {
    logObjectError(lightSensor, status);
  }
}

void registerTemperatureSensor() {
  temperatureSensor.configure(1, "Temperature");
  temperatureSensor.bindPresentValue(&temperatureValue);
  temperatureSensor.setUnits(BacnetEngineeringUnits::DegreesCelsius);
  temperatureSensor.addProperty(BacnetPropertyId::Description, "DS18B20 temperature sensor");
  temperatureSensor.addProperty(BacnetPropertyId::MinPresentValue, -55.0F);
  temperatureSensor.addProperty(BacnetPropertyId::MaxPresentValue, 125.0F);
  temperatureSensor.addProperty(BacnetPropertyId::Resolution, 0.0625F);
  temperatureSensor.addProperty(BacnetPropertyId::StatusFlags, &temperatureStatusFlags);
  temperatureSensor.addProperty(BacnetPropertyId::EventState, &temperatureEventState);
  temperatureSensor.addProperty(BacnetPropertyId::Reliability, &temperatureReliability);
  const auto status = bacnetServer.addObject(temperatureSensor);
  if (status != BacnetObjectConfigurationStatus::Ok) {
    logObjectError(temperatureSensor, status);
  }
}

void registerResetButton() {
  resetButton.configure(0, "Reset Button");
  resetButton.bindPresentValue(&resetButtonValue);
  resetButton.addProperty(BacnetPropertyId::Description, "Low-active reset button");
  resetButton.addProperty(BacnetPropertyId::StatusFlags, &resetButtonStatusFlags);
  resetButton.addProperty(BacnetPropertyId::EventState, &resetButtonEventState);
  resetButton.addProperty(BacnetPropertyId::Reliability, &resetButtonReliability);
  const auto status = bacnetServer.addObject(resetButton);
  if (status != BacnetObjectConfigurationStatus::Ok) {
    logObjectError(resetButton, status);
  }
}

void registerMidButton() {
  midButton.configure(1, "Mid Button");
  midButton.bindPresentValue(&midButtonValue);
  midButton.addProperty(BacnetPropertyId::Description, "Low-active mid button");
  midButton.addProperty(BacnetPropertyId::StatusFlags, &midButtonStatusFlags);
  midButton.addProperty(BacnetPropertyId::EventState, &midButtonEventState);
  midButton.addProperty(BacnetPropertyId::Reliability, &midButtonReliability);
  const auto status = bacnetServer.addObject(midButton);
  if (status != BacnetObjectConfigurationStatus::Ok) {
    logObjectError(midButton, status);
  }
}

void registerSetButton() {
  setButton.configure(2, "Set Button");
  setButton.bindPresentValue(&setButtonValue);
  setButton.addProperty(BacnetPropertyId::Description, "Low-active set button");
  setButton.addProperty(BacnetPropertyId::StatusFlags, &setButtonStatusFlags);
  setButton.addProperty(BacnetPropertyId::EventState, &setButtonEventState);
  setButton.addProperty(BacnetPropertyId::Reliability, &setButtonReliability);
  const auto status = bacnetServer.addObject(setButton);
  if (status != BacnetObjectConfigurationStatus::Ok) {
    logObjectError(setButton, status);
  }
}

void registerLed1() {
  led1.configure(0, "LED 1");
  led1.setRelinquishDefault(false);
  led1.attachOutput(applyLed1, nullptr);
  led1.addProperty(BacnetPropertyId::Description, "GPIO25 development-station LED");
  const auto status = bacnetServer.addObject(led1);
  if (status != BacnetObjectConfigurationStatus::Ok) {
    logObjectError(led1, status);
  }
}

void registerLed2() {
  led2.configure(1, "LED 2");
  led2.setRelinquishDefault(false);
  led2.attachOutput(applyLed2, nullptr);
  led2.addProperty(BacnetPropertyId::Description, "GPIO26 development-station LED");
  const auto status = bacnetServer.addObject(led2);
  if (status != BacnetObjectConfigurationStatus::Ok) {
    logObjectError(led2, status);
  }
}

void registerIoBindings() {
  ioManager.addAnalogInput("ldr_s", "LDR light level", 36, true, 0, 4095, 0.0F, 100.0F, "%", 1);
  ioManager.addAnalogInputToSettingsGroup("ldr_s", "I/O", "Analog Inputs", "LDR", 10);
  ioManager.addAnalogInputToLive("ldr_s", 10, "Live I/O", "Hardware Inputs", "LDR", "Light Sensor", false);
  ioManager.addAnalogInputToLive("ldr_s", 11, "Live I/O", "Hardware Inputs", "LDR", "Light Sensor raw", true);

  ioManager.addDigitalInput("reset", "Reset Button", 14, true, true, false, true);
  ioManager.addDigitalInputToSettingsGroup("reset", "I/O", "Digital Inputs", "Reset Button", 14);
  ioManager.addDigitalInputToLive("reset", 14, "Live I/O", "Hardware Inputs", "Buttons", "Reset Button", false);
  ioManager.addDigitalInput("mid", "Mid Button", 33, true, true, false, true);
  ioManager.addDigitalInputToSettingsGroup("mid", "I/O", "Digital Inputs", "Mid Button", 30);
  ioManager.addDigitalInputToLive("mid", 30, "Live I/O", "Hardware Inputs", "Buttons", "Mid Button", false);
  ioManager.addDigitalInput("set", "Set Button", 19, true, true, false, true);
  ioManager.addDigitalInputToSettingsGroup("set", "I/O", "Digital Inputs", "Set Button", 19);
  ioManager.addDigitalInputToLive("set", 19, "Live I/O", "Hardware Inputs", "Buttons", "Set Button", false);

  ioManager.addDigitalOutput("led1", "LED 1 Red", 25, false, true);
  ioManager.addDigitalOutputToSettingsGroup("led1", "I/O", "Digital Outputs", "LED 1", 50);
  ioManager.addDigitalOutput("led2", "LED 2 Yellow", 26, false, true);
  ioManager.addDigitalOutputToSettingsGroup("led2", "I/O", "Digital Outputs", "LED 2", 60);
}

void configureDs18b20() {
  dsConfigured = settings.dsEnabled->get();
  temperatureSensor.outOfService = !dsConfigured;
  if (!dsConfigured) {
    updateHealth(io_example::inputHealth(false, false), temperatureStatusFlags, temperatureEventState, temperatureReliability);
    return;
  }
  oneWire = OneWire(settings.dsGpio->get());
  ds18b20.setOneWire(&oneWire);
  ds18b20.begin();
  ds18b20.setWaitForConversion(false);
  ds18b20.requestTemperatures();
}

void configureBacnetObjects() {
  bacnetConfigured = true;
  registerLightSensor();
  registerTemperatureSensor();
  registerResetButton();
  registerMidButton();
  registerSetButton();
  registerLed1();
  registerLed2();
  bacnetServer.setClock(&bacnetClock);
  bacnetServer.setActivityListener(observeBacnetActivity);
}

void pollInputs(uint32_t now) {
  if (now - lastInputPollMs < kInputPollMs) {
    return;
  }
  lastInputPollMs = now;

  const bool ldrConfigured = ioManager.isConfigured("ldr_s");
  lightRawValue = ioManager.getAnalogRawValue("ldr_s");
  lightValue = ioManager.getAnalogValue("ldr_s");
  lightSensor.outOfService = !ldrConfigured;
  updateHealth(io_example::inputHealth(ldrConfigured, std::isfinite(lightValue)),
               lightStatusFlags,
               lightEventState,
               lightReliability);

  const bool resetConfigured = ioManager.isConfigured("reset");
  resetButtonValue = ioManager.getInputState("reset");
  resetButton.outOfService = !resetConfigured;
  updateHealth(io_example::inputHealth(resetConfigured, resetConfigured),
               resetButtonStatusFlags,
               resetButtonEventState,
               resetButtonReliability);
  const bool midConfigured = ioManager.isConfigured("mid");
  midButtonValue = ioManager.getInputState("mid");
  midButton.outOfService = !midConfigured;
  updateHealth(io_example::inputHealth(midConfigured, midConfigured),
               midButtonStatusFlags,
               midButtonEventState,
               midButtonReliability);
  const bool setConfigured = ioManager.isConfigured("set");
  setButtonValue = ioManager.getInputState("set");
  setButton.outOfService = !setConfigured;
  updateHealth(io_example::inputHealth(setConfigured, setConfigured),
               setButtonStatusFlags,
               setButtonEventState,
               setButtonReliability);

  if (dsConfigured && now - lastDsReadMs >= kDsReadIntervalMs) {
    lastDsReadMs = now;
    const float temperature = ds18b20.getTempCByIndex(0);
    const bool valid = std::isfinite(temperature) && temperature != DEVICE_DISCONNECTED_C &&
                       temperature >= -55.0F && temperature <= 125.0F;
    updateHealth(io_example::inputHealth(true, valid), temperatureStatusFlags, temperatureEventState, temperatureReliability);
    if (valid) {
      temperatureValue = temperature;
    }
    ds18b20.requestTemperatures();
  }
}

uint32_t activityAgeSeconds(uint32_t timestamp) {
  return timestamp == 0 ? 0 : (millis() - timestamp) / 1000U;
}

const char* activityStateText() {
  return bacnetActivity.seen && millis() - bacnetActivity.lastSeenMs <= kRecentActivityMs
           ? "Recent BACnet activity"
           : "No recent BACnet activity";
}

void setupRuntimeUi() {
  auto sensorLive = ConfigManager.liveGroup("ioBacnet")
                      .page("Live I/O", 10)
                      .card("External Sensor");
  sensorLive.value("temperature", []() { return temperatureValue; })
    .label("Temperature")
    .unit("°C")
    .precision(3)
    .order(10);
  sensorLive.value("lightReliability", []() { return reliabilityText(lightReliability); })
    .label("Light Sensor reliability")
    .order(20);
  sensorLive.value("temperatureReliability",
                   []() { return reliabilityText(temperatureReliability); })
    .label("Temperature reliability")
    .order(30);

  auto outputLive = ConfigManager.liveGroup("ioBacnet")
                      .page("Live I/O", 10)
                      .card("BACnet Outputs");
  outputLive.boolValue("led1Value", []() { return led1.priority.effectiveValue(); })
    .label("LED 1 BACnet Present_Value")
    .order(10);
  outputLive.boolValue("led1Io", []() { return ioManager.getStatus("led1"); })
    .label("LED 1 IOManager output state")
    .order(20);
  outputLive.value("led1Priority", []() { return led1.priority.effectivePriority(); })
    .label("LED 1 effective priority (0 = Relinquish_Default)")
    .order(30);
  outputLive.boolValue("led2Value", []() { return led2.priority.effectiveValue(); })
    .label("LED 2 BACnet Present_Value")
    .order(40);
  outputLive.boolValue("led2Io", []() { return ioManager.getStatus("led2"); })
    .label("LED 2 IOManager output state")
    .order(50);
  outputLive.value("led2Priority", []() { return led2.priority.effectivePriority(); })
    .label("LED 2 effective priority (0 = Relinquish_Default)")
    .order(60);

  auto activityLive = ConfigManager.liveGroup("bacnetActivity")
                        .page("BACnet", 20)
                        .card("BACnet Activity");
  activityLive.value("state", []() { return activityStateText(); })
    .label("BACnet activity")
    .order(10);
  activityLive.value("peer", []() { return bacnetActivity.peerText; })
    .label("Last BACnet peer")
    .order(20);
  activityLive.value("service", []() { return bacnetActivity.service == BacnetServerActivityService::ReadProperty ? "ReadProperty" : "WriteProperty"; })
    .label("Last request")
    .order(30);
  activityLive.value("object", []() { return objectText(bacnetActivity.object); })
    .label("Last object")
    .order(40);
  activityLive.value("property", []() { return propertyText(bacnetActivity.property); })
    .label("Last property")
    .order(50);
  activityLive.value("age", []() { return activityAgeSeconds(bacnetActivity.lastSeenMs); })
    .label("Last seen")
    .unit("s ago")
    .precision(0)
    .order(60);
  activityLive.value("covCount", []() { return static_cast<uint32_t>(covLive.count); })
    .label("Active COV subscriptions")
    .order(70);
  activityLive.value("covPeer", []() { return covLive.peer; }).label("COV client").order(80);
  activityLive.value("covProcess", []() { return covLive.processId; }).label("COV process ID").order(90);
  activityLive.value("covObject", []() { return covLive.object; }).label("COV object").order(100);
  activityLive.value("covProperty", []() { return covLive.property; }).label("COV property").order(110);
  activityLive.value("covMode", []() { return covLive.mode; }).label("COV mode").order(120);
  activityLive.value("covState", []() { return covLive.state; }).label("COV state").order(130);
  activityLive.value("covLifetime", []() { return covLive.lifetimeSeconds; }).label("COV lifetime").unit("s").order(140);
  activityLive.value("covLastSent", []() { return activityAgeSeconds(covLive.lastSentMs); }).label("COV last sent").unit("s ago").precision(0).order(150);
  activityLive.value("covLastAck", []() { return activityAgeSeconds(covLive.lastAckMs); }).label("COV last ACK").unit("s ago").precision(0).order(160);

  auto pointLive = ConfigManager.liveGroup("bacnetActivity")
                     .page("BACnet", 20)
                     .card("BACnet Data Point Activity");
  pointLive.value("lightReads", []() { return lightActivity.reads; }).label("AI0 Light Sensor reads").order(10);
  pointLive.value("lightReadAge", []() { return activityAgeSeconds(lightActivity.lastReadMs); }).label("AI0 last read").unit("s ago").precision(0).order(20);
  pointLive.value("temperatureReads", []() { return temperatureActivity.reads; }).label("AI1 Temperature reads").order(30);
  pointLive.value("temperatureReadAge", []() { return activityAgeSeconds(temperatureActivity.lastReadMs); }).label("AI1 last read").unit("s ago").precision(0).order(40);
  pointLive.value("led1Writes", []() { return led1Activity.writes; }).label("BO0 LED 1 writes").order(50);
  pointLive.value("led1WritePriority", []() { return led1Activity.lastWritePriority; }).label("BO0 last write priority").order(60);
  pointLive.value("led2Writes", []() { return led2Activity.writes; }).label("BO1 LED 2 writes").order(70);
  pointLive.value("led2WritePriority", []() { return led2Activity.lastWritePriority; }).label("BO1 last write priority").order(80);
}

void startBacnetWhenConnected() {
  if (bacnetBound || !bacnetConfigured) {
    return;
  }
  const BacnetServerDevice device{deviceInstance, vendorId, "ESP32 I/O BACnet Server", "Unregistered BACnet Test Server", "ESP32 I/O Server", kVersion, nullptr};
  bacnetBound = bacnetServer.begin(device, udpPort);
  Serial.println(bacnetBound ? "[I] BACnet server online" : "[E] BACnet UDP bind failed");
}

void setupNetworkDefaults() {
  if (!coreSettings.wifi.wifiSsid.get().isEmpty()) {
    return;
  }
  Serial.println("[W] WiFi settings are empty; configure them in ConfigManager");
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
  registerIoBindings();
  ConfigManager.loadAll();
  ioManager.begin();
  ioManager.set("led1", false);
  ioManager.set("led2", false);
  deviceInstance = static_cast<uint32_t>(settings.deviceInstance->get());
  udpPort = static_cast<uint16_t>(settings.udpPort->get());
  const int configuredVendorId = settings.vendorId->get();
  if (deviceInstance > 0x003FFFFFU || udpPort == 0 || configuredVendorId < 0 ||
      configuredVendorId > UINT16_MAX) {
    Serial.println("[E] Invalid BACnet startup settings; using defaults");
    deviceInstance = kDeviceInstanceDefault;
    udpPort = BacnetServer::kDefaultPort;
    vendorId = kDevelopmentVendorId;
  } else {
    vendorId = static_cast<uint16_t>(configuredVendorId);
  }
  configureDs18b20();
  configureBacnetObjects();
  setupRuntimeUi();
  setupNetworkDefaults();
  ConfigManager.startWebServer();
  // Keep live I/O feedback responsive; ConfigManager defaults to a slower push interval.
  ConfigManager.setWebSocketInterval(550);
}

void loop() {
  const uint32_t now = millis();
  ConfigManager.getWiFiManager().update();
  ioManager.update();
  ConfigManager.handleClient();
  pollInputs(now);
  if (bacnetBound) {
    static_cast<void>(bacnetServer.poll());
  }
  refreshCovLiveState();
}

void onWiFiConnected() {
  wifiServices.onConnected(ConfigManager, kAppName, coreSettings.system, coreSettings.ntp);
  Serial.print("[I] WiFi connected, local IP ");
  Serial.println(WiFi.localIP());
  startBacnetWhenConnected();
}

void onWiFiDisconnected() {
  wifiServices.onDisconnected();
  if (bacnetBound) {
    bacnetServer.end();
  }
  bacnetBound = false;
}

void onWiFiAPMode() {
  wifiServices.onAPMode();
}
