// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <Arduino.h>

#include <BME280_I2C.h>
#include <EspBacnet.h>
#include <Ticker.h>
#include <WiFi.h>

#include "ConfigManager.h"
#include "alarm/AlarmManager.h"
#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"
#include "helpers/HelperModule.h"
#include "logging/LoggingManager.h"

#include <cstdarg>
#include <cstdio>
#include <memory>

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#define CM_HAS_WIFI_SECRETS 1
#else
#define CM_HAS_WIFI_SECRETS 0
#endif

#ifndef BME280_ADDRESS
#define BME280_ADDRESS 0x76
#endif

#define VERSION CONFIGMANAGER_VERSION
#ifndef APP_NAME
#define APP_NAME "BACnet Client Discovery Demo"
#endif

#ifndef SETTINGS_PASSWORD
#define SETTINGS_PASSWORD ""
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD SETTINGS_PASSWORD
#endif

// I2C pins for the optional BME280 sensor.
#define I2C_SDA 21
#define I2C_SCL 22

extern ConfigManagerClass ConfigManager;

static cm::CoreSettings& coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings& systemSettings = coreSettings.system;
static cm::CoreWiFiSettings& wifiSettings = coreSettings.wifi;
static cm::CoreNtpSettings& ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;
static cm::AlarmManager alarmManager;

static BME280_I2C bme280;
static Ticker temperatureTicker;
static BacnetClient bacnetClient;

static const IPAddress kWhoIsDestination(192, 168, 2, 255);
static const IPAddress kWagoAddress(192, 168, 2, 101);
static constexpr uint32_t kWagoDeviceInstance = 9001;
static constexpr uint32_t kWhoIsIntervalMs = 30000;
static constexpr uint32_t kReadPropertyTimeoutMs = 5000;
static constexpr uint32_t kBacnetScanReadTimeoutMs = 700;
static constexpr uint32_t kBacnetScanProbeDelayMs = 25;
static constexpr uint32_t kBacnetScanProgressInterval = 100;
static constexpr uint32_t kBacnetPresentValueRefreshMs = 30000;
static constexpr size_t kMaxBacnetDevices = 5;
static constexpr size_t kMaxBacnetProperties = 5;
static constexpr size_t kMaxBacnetAnalogValues = 10;
static constexpr size_t kMaxBacnetMultiStateValues = 10;
static constexpr size_t kBacnetPreviewPropertyCount = 4;
static constexpr uint16_t kBacnetDeviceObjectType =
    static_cast<uint16_t>(BacnetObjectType::Device);
static constexpr uint16_t kBacnetAnalogValueObjectType =
    static_cast<uint16_t>(BacnetObjectType::AnalogValue);
static constexpr uint16_t kBacnetMultiStateValueObjectType =
    static_cast<uint16_t>(BacnetObjectType::MultiStateValue);
static constexpr uint32_t kAnalogValueScanStart = 200;
static constexpr uint32_t kAnalogValueScanEnd = 299;
static constexpr uint32_t kMultiStateValueScanStart = 2000;
static constexpr uint32_t kMultiStateValueScanEnd = 2099;

static float temperature = 0.0f;
static float dewPoint = 0.0f;
static float humidity = 0.0f;
static float pressure = 0.0f;
static bool bacnetStarted = false;
static bool receivedIAmSinceLastWhoIs = true;
static uint32_t whoIsCount = 0;
static unsigned long lastWhoIsAt = 0;

struct BacnetPropertyPreview {
  const char* name = "";
  BacnetPropertyId id = BacnetPropertyId::ObjectName;
  String value = "";
  String status = "not read";
  bool requested = false;
  bool received = false;
  bool failed = false;
};

struct BacnetValueObjectPreview {
  bool discovered = false;
  BacnetObjectId object;
  String objectName = "";
  String description = "";
  String label = "";
  String presentValue = "";
  String presentValueStatus = "pending";
  unsigned long lastPresentValueRefreshAt = 0;
};

struct BacnetPropertySpec {
  const char* name = "";
  BacnetPropertyId id = BacnetPropertyId::ObjectName;
};

static constexpr BacnetPropertySpec
    kBacnetPreviewProperties[kBacnetPreviewPropertyCount] = {
        {"objectName", BacnetPropertyId::ObjectName},
        {"vendorName", BacnetPropertyId::VendorName},
        {"modelName", BacnetPropertyId::ModelName},
        {"firmwareRevision", BacnetPropertyId::FirmwareRevision},
};

struct BacnetDevicePreview {
  bool active = false;
  uint32_t deviceId = 0;
  IPAddress address;
  uint16_t vendorId = 0;
  uint32_t maxApdu = 0;
  uint8_t segmentation = 0;
  BacnetPropertyPreview properties[kMaxBacnetProperties];
  BacnetValueObjectPreview analogValues[kMaxBacnetAnalogValues];
  BacnetValueObjectPreview multiStateValues[kMaxBacnetMultiStateValues];
  bool valueScanStarted = false;
  bool valueScanFinished = false;
  bool analogScanFinished = false;
  bool multiStateScanFinished = false;
  uint32_t nextAnalogValueInstance = kAnalogValueScanStart;
  uint32_t nextMultiStateValueInstance = kMultiStateValueScanStart;
  unsigned long lastScanProbeAt = 0;
};

enum class BacnetReadTarget {
  None,
  DeviceProperty,
  ScanObjectName,
  ScanDescription,
  ScanPresentValue,
  RefreshPresentValue,
};

static BacnetDevicePreview bacnetDevices[kMaxBacnetDevices];
static BacnetReadTarget activeReadTarget = BacnetReadTarget::None;
static int activeReadDeviceIndex = -1;
static int activeReadPropertyIndex = -1;
static int activeReadValueIndex = -1;
static uint16_t activeReadObjectType = 0;
static uint32_t activeReadObjectInstance = 0;
static BacnetPropertyRequest activeReadRequest;
static uint8_t activeReadInvokeId = 0;
static uint8_t nextReadInvokeId = 1;
static unsigned long activeReadStartedAt = 0;
static uint32_t activeReadTimeoutMs = kReadPropertyTimeoutMs;

static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
.myCSSTempClass { color:rgb(198, 16, 16) !important; font-weight:900!important; font-size: 1.2rem!important; }
)CSS";

void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();
static void setupNetworkDefaults();

using LogLevel = cm::LoggingManager::Level;

static void bacnetGuiLog(LogLevel level, const char* format, ...) {
  char message[160] = {};
  va_list args;
  va_start(args, format);
  std::vsnprintf(message, sizeof(message), format, args);
  va_end(args);

  cm::LoggingManager::instance().logTag(level, "BACnet", "%s", message);
}

static void setupGuiLogging() {
#if !CM_DISABLE_GUI_LOGGING
  auto guiOut = std::make_unique<cm::LoggingManager::GuiOutput>(
      ConfigManager, 80);
  guiOut->setLevel(LogLevel::Info);
  guiOut->setMaxQueue(120);
  guiOut->setMaxPerTick(8);
  guiOut->addTimestamp(
      cm::LoggingManager::Output::TimestampMode::Millis);
  cm::LoggingManager::instance().addOutput(std::move(guiOut));
#endif
  cm::LoggingManager::instance().setGlobalLevel(LogLevel::Info);
}

struct TempSettings {
  Config<float>* tempCorrection = nullptr;
  Config<float>* humidityCorrection = nullptr;
  Config<int>* seaLevelPressure = nullptr;
  Config<int>* readIntervalSec = nullptr;

  void create() {
    tempCorrection = &ConfigManager.addSettingFloat("TCO")
                          .name("Temperature Correction")
                          .category("Temp")
                          .defaultValue(0.0f)
                          .build();
    humidityCorrection = &ConfigManager.addSettingFloat("HYO")
                             .name("Humidity Correction")
                             .category("Temp")
                             .defaultValue(0.0f)
                             .build();
    seaLevelPressure = &ConfigManager.addSettingInt("SLP")
                            .name("Sea Level Pressure")
                            .category("Temp")
                            .defaultValue(1013)
                            .build();
    readIntervalSec = &ConfigManager.addSettingInt("ReadTemp")
                           .name("Read Temp/Humidity every (s)")
                           .category("Temp")
                           .defaultValue(30)
                           .build();
  }

  void placeInUi() const {
    if (!tempCorrection || !humidityCorrection || !seaLevelPressure ||
        !readIntervalSec) {
      return;
    }

    ConfigManager.addSettingsPage("Temp", 40);
    ConfigManager.addSettingsGroup("Temp", "Temp", "Temperature", 40);
    ConfigManager.addToSettingsGroup(tempCorrection->getKey(), "Temp", "Temp",
                                     "Temperature", 10);
    ConfigManager.addToSettingsGroup(humidityCorrection->getKey(), "Temp",
                                     "Temp", "Temperature", 20);
    ConfigManager.addToSettingsGroup(seaLevelPressure->getKey(), "Temp", "Temp",
                                     "Temperature", 30);
    ConfigManager.addToSettingsGroup(readIntervalSec->getKey(), "Temp", "Temp",
                                     "Temperature", 40);
  }
};

static TempSettings tempSettings;

static void initializeBacnetProperties(BacnetDevicePreview& device) {
  if (device.properties[0].name[0] != '\0') {
    return;
  }

  for (size_t i = 0; i < kBacnetPreviewPropertyCount; ++i) {
    device.properties[i].name = kBacnetPreviewProperties[i].name;
    device.properties[i].id = kBacnetPreviewProperties[i].id;
  }
}

static String bacnetDeviceSummary(size_t index) {
  if (index >= kMaxBacnetDevices || !bacnetDevices[index].active) {
    return "No device discovered";
  }

  const BacnetDevicePreview& device = bacnetDevices[index];
  String summary = "ID ";
  summary += String(device.deviceId);
  summary += " @ ";
  summary += device.address == IPAddress(0, 0, 0, 0) ? "unknown"
                                                     : device.address.toString();
  summary += " vendor ";
  summary += String(device.vendorId);
  return summary;
}

static String bacnetPropertySummary(size_t deviceIndex, size_t propertyIndex) {
  if (deviceIndex >= kMaxBacnetDevices ||
      propertyIndex >= kBacnetPreviewPropertyCount ||
      !bacnetDevices[deviceIndex].active) {
    return "not discovered";
  }

  const BacnetPropertyPreview& property =
      bacnetDevices[deviceIndex].properties[propertyIndex];
  if (property.received) {
    return property.value;
  }
  return property.status;
}

static String shortenBacnetLabel(const String& label) {
  static constexpr size_t kMaxLabelLength = 36;
  if (label.length() <= kMaxLabelLength) {
    return label;
  }
  return label.substring(0, kMaxLabelLength - 3) + "...";
}

static String bacnetValueObjectRow(const BacnetValueObjectPreview& object,
                                   const char* prefix) {
  String row = prefix;
  row += String(object.object.instance);
  row += ": ";
  row += shortenBacnetLabel(object.label.length() ? object.label
                                                  : String("<unnamed>"));
  row += " - V: ";
  row += object.presentValue.length() ? object.presentValue
                                      : object.presentValueStatus;
  return row;
}

static String bacnetValueObjectsSummary(const BacnetValueObjectPreview* objects,
                                        size_t count, const char* prefix,
                                        bool scanFinished) {
  String summary;
  for (size_t i = 0; i < count; ++i) {
    if (!objects[i].discovered) {
      continue;
    }
    if (summary.length()) {
      summary += "\n";
    }
    summary += bacnetValueObjectRow(objects[i], prefix);
  }

  if (summary.length()) {
    return summary;
  }
  return scanFinished ? "No objects found" : "Scanning...";
}

static String bacnetAnalogValuesSummary(size_t deviceIndex) {
  if (deviceIndex >= kMaxBacnetDevices || !bacnetDevices[deviceIndex].active) {
    return "No device discovered";
  }
  const BacnetDevicePreview& device = bacnetDevices[deviceIndex];
  return bacnetValueObjectsSummary(device.analogValues, kMaxBacnetAnalogValues,
                                   "AV", device.analogScanFinished);
}

static String bacnetMultiStateValuesSummary(size_t deviceIndex) {
  if (deviceIndex >= kMaxBacnetDevices || !bacnetDevices[deviceIndex].active) {
    return "No device discovered";
  }
  const BacnetDevicePreview& device = bacnetDevices[deviceIndex];
  return bacnetValueObjectsSummary(device.multiStateValues,
                                   kMaxBacnetMultiStateValues, "MV",
                                   device.multiStateScanFinished);
}

static void setupRuntimeUI() {
  auto live = ConfigManager.liveGroup("sensors")
                  .page("Sensors", 10)
                  .card("BME280 - Temperature Sensor");

  live.value("temp", []() { return temperature; })
      .label("Temperature")
      .unit("C")
      .precision(1)
      .addCSSClass("myCSSTempClass")
      .order(10);

  live.value("hum", []() { return humidity; })
      .label("Humidity")
      .unit("%")
      .precision(1)
      .order(11);

  live.value("pressure", []() { return pressure; })
      .label("Pressure")
      .unit("hPa")
      .precision(1)
      .order(12);

  auto dewpointGroup = ConfigManager.liveGroup("sensors")
                           .page("Sensors", 10)
                           .card("BME280 - Temperature Sensor")
                           .group("Dewpoint", 20);

  dewpointGroup.value("dew", []() { return dewPoint; })
      .label("Dewpoint")
      .unit("C")
      .precision(1)
      .order(20);

  alarmManager.addDigitalWarning({
      .id = "dewRisk",
      .name = "Condensation Risk",
      .kind = cm::AlarmKind::DigitalActive,
      .severity = cm::AlarmSeverity::Warning,
      .enabled = true,
      .getter = []() { return temperature < dewPoint; },
  });

  alarmManager.addWarningToLive("dewRisk", 30, "Sensors",
                                "BME280 - Temperature Sensor", "Dewpoint",
                                "Condensation Risk");

  for (size_t deviceIndex = 0; deviceIndex < 1; ++deviceIndex) {
    const String groupName = "Discovered Device";
    auto bacnetDeviceGroup = ConfigManager.liveGroup("bacnet")
                                  .page("Sensors", 10)
                                  .card("BACnet/IP Discovery", 20)
                                  .group(groupName.c_str(), deviceIndex + 1);

    const String deviceKey = String("device") + String(deviceIndex);
    bacnetDeviceGroup
        .value(deviceKey.c_str(),
               [deviceIndex]() { return bacnetDeviceSummary(deviceIndex); })
        .label("Device")
        .order(10);

    for (size_t propertyIndex = 0; propertyIndex < kBacnetPreviewPropertyCount;
         ++propertyIndex) {
      const String propertyKey = String("device") + String(deviceIndex) +
                                 "_" +
                                 kBacnetPreviewProperties[propertyIndex].name;
      bacnetDeviceGroup
          .value(propertyKey.c_str(), [deviceIndex, propertyIndex]() {
            return bacnetPropertySummary(deviceIndex, propertyIndex);
          })
          .label(kBacnetPreviewProperties[propertyIndex].name)
          .order(20 + propertyIndex);
    }

    bacnetDeviceGroup.divider("Analog Values", 39);
    bacnetDeviceGroup
        .value("device0_analogValues",
               [deviceIndex]() { return bacnetAnalogValuesSummary(deviceIndex); })
        .label("AV")
        .order(40);

    bacnetDeviceGroup.divider("Multi-State Values", 59);
    bacnetDeviceGroup
        .value("device0_multiStateValues",
               [deviceIndex]() {
                 return bacnetMultiStateValuesSummary(deviceIndex);
               })
        .label("MV")
        .order(60);
  }
}

static void readBme280() {
  bme280.setSeaLevelPressure(tempSettings.seaLevelPressure->get());
  bme280.read();

  temperature = bme280.data.temperature + tempSettings.tempCorrection->get();
  humidity = bme280.data.humidity + tempSettings.humidityCorrection->get();
  pressure = bme280.data.pressure;
  dewPoint = cm::helpers::computeDewPoint(temperature, humidity);
}

static void setupTemperatureMeasuring() {
  Serial.println("[I] Initializing BME280 sensor");

  bme280.setAddress(BME280_ADDRESS, I2C_SDA, I2C_SCL);

  const bool ok = bme280.begin(
      bme280.BME280_STANDBY_0_5, bme280.BME280_FILTER_OFF,
      bme280.BME280_SPI3_DISABLE, bme280.BME280_OVERSAMPLING_1,
      bme280.BME280_OVERSAMPLING_1, bme280.BME280_OVERSAMPLING_1,
      bme280.BME280_MODE_NORMAL);

  if (!ok) {
    Serial.println("[W] BME280 not initialized, continuing without sensor");
    return;
  }

  Serial.println("[I] BME280 ready");
  int interval = tempSettings.readIntervalSec->get();
  if (interval < 2) {
    interval = 2;
  }
  temperatureTicker.attach(static_cast<float>(interval), readBme280);
  readBme280();
}

static void sendWhoIs() {
  if (!bacnetStarted) {
    return;
  }

  if (!receivedIAmSinceLastWhoIs) {
    Serial.println("[W] No BACnet I-Am received since previous Who-Is");
    bacnetGuiLog(LogLevel::Warn,
                 "No I-Am received since previous Who-Is");
  }

  ++whoIsCount;
  Serial.print("[I] Sending BACnet Who-Is #");
  Serial.print(whoIsCount);
  Serial.print(" to ");
  Serial.print(kWhoIsDestination);
  Serial.print(":");
  Serial.println(BacnetClient::kDefaultPort);

  if (bacnetClient.sendWhoIs(kWhoIsDestination)) {
    Serial.println("[I] BACnet Who-Is sent");
    bacnetGuiLog(LogLevel::Info, "Who-Is sent to %s:%u",
                 kWhoIsDestination.toString().c_str(),
                 BacnetClient::kDefaultPort);
  } else {
    Serial.println("[W] BACnet Who-Is send failed");
    bacnetGuiLog(LogLevel::Warn, "Who-Is send failed to %s:%u",
                 kWhoIsDestination.toString().c_str(),
                 BacnetClient::kDefaultPort);
  }

  receivedIAmSinceLastWhoIs = false;
  lastWhoIsAt = millis();
}

static void startBacnetDiscovery() {
  if (bacnetStarted) {
    return;
  }

  if (!bacnetClient.begin()) {
    Serial.println("[E] BACnet UDP listener failed to start");
    bacnetGuiLog(LogLevel::Error, "client UDP listener failed");
    return;
  }

  bacnetStarted = true;
  receivedIAmSinceLastWhoIs = true;
  Serial.print("[I] BACnet client started on UDP port ");
  Serial.println(bacnetClient.localPort());
  Serial.print("[I] BACnet Who-Is destination ");
  Serial.print(kWhoIsDestination);
  Serial.print(":");
  Serial.println(BacnetClient::kDefaultPort);
  bacnetGuiLog(LogLevel::Info, "client started on UDP %u",
               bacnetClient.localPort());
  bacnetGuiLog(LogLevel::Info, "Who-Is destination %s:%u",
               kWhoIsDestination.toString().c_str(),
               BacnetClient::kDefaultPort);

  sendWhoIs();
}

static int findBacnetDeviceSlot(uint32_t deviceId) {
  for (size_t i = 0; i < kMaxBacnetDevices; ++i) {
    if (bacnetDevices[i].active && bacnetDevices[i].deviceId == deviceId) {
      return static_cast<int>(i);
    }
  }

  for (size_t i = 0; i < kMaxBacnetDevices; ++i) {
    if (!bacnetDevices[i].active) {
      return static_cast<int>(i);
    }
  }

  return -1;
}

static int recordBacnetDevice(const BacnetIAmDevice& iAm) {
  const int slot = findBacnetDeviceSlot(iAm.deviceInstance);
  if (slot < 0) {
    Serial.println("[W] BACnet device preview list is full");
    bacnetGuiLog(LogLevel::Warn, "device preview list full");
    return -1;
  }

  BacnetDevicePreview& device = bacnetDevices[slot];
  const bool isNewDevice = !device.active;
  device.active = true;
  device.deviceId = iAm.deviceInstance;
  device.address = iAm.deviceInstance == kWagoDeviceInstance
                       ? kWagoAddress
                       : IPAddress(0, 0, 0, 0);
  device.vendorId = iAm.vendorId;
  device.maxApdu = iAm.maxApduLengthAccepted;
  device.segmentation = iAm.segmentationSupported;
  initializeBacnetProperties(device);

  if (isNewDevice) {
    for (size_t i = 0; i < kBacnetPreviewPropertyCount; ++i) {
      device.properties[i].status = "pending";
    }
  }

  return slot;
}

static bool isUsefulBacnetLabel(const String& label, uint16_t objectType,
                                uint32_t instance) {
  String normalized = label;
  normalized.trim();
  if (normalized.isEmpty()) {
    return false;
  }

  String generic;
  if (objectType == kBacnetAnalogValueObjectType) {
    generic = String("analog-value,") + String(instance);
  } else if (objectType == kBacnetMultiStateValueObjectType) {
    generic = String("multi-state-value,") + String(instance);
  }
  normalized.toLowerCase();
  generic.toLowerCase();
  return normalized != generic;
}

static String selectBacnetLabel(const BacnetValueObjectPreview& object) {
  if (isUsefulBacnetLabel(object.objectName, object.object.type,
                          object.object.instance)) {
    return object.objectName;
  }
  if (isUsefulBacnetLabel(object.description, object.object.type,
                          object.object.instance)) {
    return object.description;
  }
  return "<unnamed>";
}

static BacnetValueObjectPreview* valueObjectsForType(BacnetDevicePreview& device,
                                                     uint16_t objectType,
                                                     size_t& limit) {
  if (objectType == kBacnetAnalogValueObjectType) {
    limit = kMaxBacnetAnalogValues;
    return device.analogValues;
  }
  if (objectType == kBacnetMultiStateValueObjectType) {
    limit = kMaxBacnetMultiStateValues;
    return device.multiStateValues;
  }
  limit = 0;
  return nullptr;
}

static size_t discoveredValueObjectCount(const BacnetDevicePreview& device,
                                         uint16_t objectType) {
  size_t count = 0;
  const BacnetValueObjectPreview* objects = nullptr;
  size_t limit = 0;
  if (objectType == kBacnetAnalogValueObjectType) {
    objects = device.analogValues;
    limit = kMaxBacnetAnalogValues;
  } else if (objectType == kBacnetMultiStateValueObjectType) {
    objects = device.multiStateValues;
    limit = kMaxBacnetMultiStateValues;
  }

  for (size_t i = 0; i < limit; ++i) {
    if (objects[i].discovered) {
      ++count;
    }
  }
  return count;
}

static uint8_t allocateReadPropertyInvokeId() {
  const uint8_t invokeId = nextReadInvokeId;
  ++nextReadInvokeId;
  if (nextReadInvokeId == 0) {
    nextReadInvokeId = 1;
  }
  return invokeId;
}

static const char* objectTypeName(uint16_t objectType) {
  return objectType == kBacnetAnalogValueObjectType ? "analog-value"
                                                    : "multi-state-value";
}

static bool shouldLogScanProgress(uint16_t objectType, uint32_t instance) {
  const uint32_t start = objectType == kBacnetAnalogValueObjectType
                             ? kAnalogValueScanStart
                             : kMultiStateValueScanStart;
  return instance == start ||
         ((instance - start) % kBacnetScanProgressInterval) == 0;
}

static void markValueScanFinished(BacnetDevicePreview& device,
                                  uint16_t objectType) {
  if (objectType == kBacnetAnalogValueObjectType) {
    device.analogScanFinished = true;
    if (discoveredValueObjectCount(device, objectType) == 0) {
      Serial.println("[W] BACnet scan found no analog-value objects");
      bacnetGuiLog(LogLevel::Warn, "Analog Values found: 0");
    }
  } else {
    device.multiStateScanFinished = true;
    if (discoveredValueObjectCount(device, objectType) == 0) {
      Serial.println("[W] BACnet scan found no multi-state-value objects");
      bacnetGuiLog(LogLevel::Warn, "Multi-State Values found: 0");
    }
  }

  if (device.analogScanFinished && device.multiStateScanFinished &&
      !device.valueScanFinished) {
    device.valueScanFinished = true;
    const size_t avCount =
        discoveredValueObjectCount(device, kBacnetAnalogValueObjectType);
    const size_t mvCount =
        discoveredValueObjectCount(device, kBacnetMultiStateValueObjectType);
    Serial.print("[I] BACnet scan finished. Analog Values found: ");
    Serial.print(avCount);
    Serial.print(", Multi-State Values found: ");
    Serial.println(mvCount);
    bacnetGuiLog(LogLevel::Info,
                 "scan finished. Analog Values found: %u, "
                 "Multi-State Values found: %u",
                 static_cast<unsigned>(avCount),
                 static_cast<unsigned>(mvCount));
  }
}

static bool sendReadPropertyRequest(BacnetDevicePreview& device,
                                    const BacnetPropertyRequest& request,
                                    BacnetReadTarget target,
                                    uint32_t timeoutMs) {
  activeReadTarget = target;
  activeReadInvokeId = allocateReadPropertyInvokeId();
  activeReadStartedAt = millis();
  activeReadTimeoutMs = timeoutMs;
  activeReadRequest = request;
  if (!bacnetClient.sendReadProperty(device.address, request,
                                     activeReadInvokeId)) {
    activeReadTarget = BacnetReadTarget::None;
    return false;
  }
  return true;
}

static void clearActiveReadState() {
  activeReadTarget = BacnetReadTarget::None;
  activeReadDeviceIndex = -1;
  activeReadPropertyIndex = -1;
  activeReadValueIndex = -1;
  activeReadObjectType = 0;
  activeReadObjectInstance = 0;
  activeReadRequest = BacnetPropertyRequest{};
  activeReadTimeoutMs = kReadPropertyTimeoutMs;
}

static bool startValueObjectProbe(size_t deviceIndex, uint16_t objectType,
                                  uint32_t instance) {
  BacnetDevicePreview& device = bacnetDevices[deviceIndex];
  activeReadDeviceIndex = static_cast<int>(deviceIndex);
  activeReadPropertyIndex = -1;
  activeReadValueIndex = -1;
  activeReadObjectType = objectType;
  activeReadObjectInstance = instance;
  const BacnetPropertyRequest request{
      BacnetObjectId{objectType, instance}, BacnetPropertyId::ObjectName};

  if (shouldLogScanProgress(objectType, instance)) {
    Serial.print("[I] BACnet scan progress ");
    Serial.print(objectTypeName(objectType));
    Serial.print(",");
    Serial.println(instance);
    bacnetGuiLog(LogLevel::Info, "scan progress %s,%lu",
                 objectTypeName(objectType),
                 static_cast<unsigned long>(instance));
  }

  if (!sendReadPropertyRequest(device, request,
                               BacnetReadTarget::ScanObjectName,
                               kBacnetScanReadTimeoutMs)) {
    Serial.print("[W] BACnet scan send failed ");
    Serial.print(objectTypeName(objectType));
    Serial.print(",");
    Serial.println(instance);
    bacnetGuiLog(LogLevel::Warn, "%s,%lu objectName send failed",
                 objectTypeName(objectType),
                 static_cast<unsigned long>(instance));
    return false;
  }
  Serial.print("[I] BACnet scan probe sent ");
  Serial.print(objectTypeName(objectType));
  Serial.print(",");
  Serial.print(instance);
  Serial.print(" objectName invoke ");
  Serial.println(activeReadInvokeId);
  return true;
}

static bool startNextValueScanProbe(size_t deviceIndex) {
  BacnetDevicePreview& device = bacnetDevices[deviceIndex];
  if (!device.valueScanStarted) {
    device.valueScanStarted = true;
    Serial.print("[I] BACnet value scan start device ");
    Serial.print(device.deviceId);
    Serial.print(" at ");
    Serial.println(device.address);
    bacnetGuiLog(LogLevel::Info, "scan start device %lu at %s",
                 static_cast<unsigned long>(device.deviceId),
                 device.address.toString().c_str());
    bacnetGuiLog(LogLevel::Info, "scan analog-value range %lu..%lu",
                 static_cast<unsigned long>(kAnalogValueScanStart),
                 static_cast<unsigned long>(kAnalogValueScanEnd));
    bacnetGuiLog(LogLevel::Info, "scan multi-state-value range %lu..%lu",
                 static_cast<unsigned long>(kMultiStateValueScanStart),
                 static_cast<unsigned long>(kMultiStateValueScanEnd));
  }

  if (millis() - device.lastScanProbeAt < kBacnetScanProbeDelayMs) {
    return false;
  }

  if (!device.analogScanFinished) {
    if (discoveredValueObjectCount(device, kBacnetAnalogValueObjectType) >=
            kMaxBacnetAnalogValues ||
        device.nextAnalogValueInstance > kAnalogValueScanEnd) {
      markValueScanFinished(device, kBacnetAnalogValueObjectType);
      return false;
    }

    device.lastScanProbeAt = millis();
    return startValueObjectProbe(deviceIndex, kBacnetAnalogValueObjectType,
                                 device.nextAnalogValueInstance++);
  }

  if (!device.multiStateScanFinished) {
    if (discoveredValueObjectCount(device, kBacnetMultiStateValueObjectType) >=
            kMaxBacnetMultiStateValues ||
        device.nextMultiStateValueInstance > kMultiStateValueScanEnd) {
      markValueScanFinished(device, kBacnetMultiStateValueObjectType);
      return false;
    }

    device.lastScanProbeAt = millis();
    return startValueObjectProbe(deviceIndex, kBacnetMultiStateValueObjectType,
                                 device.nextMultiStateValueInstance++);
  }

  return false;
}

static bool startValueObjectPresentValueRead(size_t deviceIndex,
                                             uint16_t objectType,
                                             size_t valueIndex,
                                             BacnetReadTarget target) {
  BacnetDevicePreview& device = bacnetDevices[deviceIndex];
  size_t limit = 0;
  BacnetValueObjectPreview* objects =
      valueObjectsForType(device, objectType, limit);
  if (objects == nullptr || valueIndex >= limit || !objects[valueIndex].discovered) {
    return false;
  }

  BacnetValueObjectPreview& object = objects[valueIndex];
  activeReadDeviceIndex = static_cast<int>(deviceIndex);
  activeReadValueIndex = static_cast<int>(valueIndex);
  activeReadPropertyIndex = -1;
  activeReadObjectType = objectType;
  activeReadObjectInstance = object.object.instance;
  object.presentValueStatus = "reading";

  Serial.print("[I] Sending BACnet ReadProperty ");
  Serial.print(objectTypeName(objectType));
  Serial.print(",");
  Serial.print(object.object.instance);
  Serial.println(" presentValue");
  bacnetGuiLog(LogLevel::Info,
               "ReadProperty %s,%lu presentValue sent invoke next",
               objectTypeName(objectType),
               static_cast<unsigned long>(object.object.instance));

  const BacnetPropertyRequest request{object.object,
                                      BacnetPropertyId::PresentValue};
  if (!sendReadPropertyRequest(device, request,
                               target, kReadPropertyTimeoutMs)) {
    object.presentValueStatus = "send failed";
    bacnetGuiLog(LogLevel::Warn, "%s,%lu presentValue send failed",
                 objectTypeName(objectType),
                 static_cast<unsigned long>(object.object.instance));
    return false;
  }
  return true;
}

static bool startValueObjectDescriptionRead(size_t deviceIndex,
                                            uint16_t objectType,
                                            size_t valueIndex) {
  BacnetDevicePreview& device = bacnetDevices[deviceIndex];
  size_t limit = 0;
  BacnetValueObjectPreview* objects =
      valueObjectsForType(device, objectType, limit);
  if (objects == nullptr || valueIndex >= limit || !objects[valueIndex].discovered) {
    return false;
  }

  activeReadDeviceIndex = static_cast<int>(deviceIndex);
  activeReadValueIndex = static_cast<int>(valueIndex);
  activeReadPropertyIndex = -1;
  activeReadObjectType = objectType;
  activeReadObjectInstance = objects[valueIndex].object.instance;
  const BacnetPropertyRequest request{objects[valueIndex].object,
                                      BacnetPropertyId::Description};
  if (!sendReadPropertyRequest(device, request,
                               BacnetReadTarget::ScanDescription,
                               kReadPropertyTimeoutMs)) {
    return false;
  }
  return true;
}

static void requestNextBacnetProperty() {
  if (activeReadTarget != BacnetReadTarget::None || !bacnetStarted) {
    return;
  }

  for (size_t deviceIndex = 0; deviceIndex < kMaxBacnetDevices; ++deviceIndex) {
    BacnetDevicePreview& device = bacnetDevices[deviceIndex];
    if (!device.active || device.address == IPAddress(0, 0, 0, 0)) {
      continue;
    }

    for (size_t propertyIndex = 0; propertyIndex < kBacnetPreviewPropertyCount;
         ++propertyIndex) {
      BacnetPropertyPreview& property = device.properties[propertyIndex];
      if (property.requested || property.received || property.failed) {
        continue;
      }

      activeReadDeviceIndex = static_cast<int>(deviceIndex);
      activeReadPropertyIndex = static_cast<int>(propertyIndex);
      activeReadValueIndex = -1;
      activeReadObjectType = kBacnetDeviceObjectType;
      activeReadObjectInstance = device.deviceId;
      property.requested = true;
      property.status = "reading";

      Serial.print("[I] Sending BACnet ReadProperty ");
      Serial.print(property.name);
      Serial.print(" to ");
      Serial.print(device.address);
      Serial.print(":");
      Serial.println(BacnetClient::kDefaultPort);
      bacnetGuiLog(LogLevel::Info,
                   "ReadProperty device,%lu %s sent invoke next",
                   static_cast<unsigned long>(device.deviceId), property.name);

      const BacnetPropertyRequest request{
          BacnetObjectId{kBacnetDeviceObjectType, device.deviceId},
          property.id};
      if (!sendReadPropertyRequest(device, request,
                                   BacnetReadTarget::DeviceProperty,
                                   kReadPropertyTimeoutMs)) {
        property.failed = true;
        property.status = "send failed";
        bacnetGuiLog(LogLevel::Warn, "ReadProperty device,%lu %s send failed",
                     static_cast<unsigned long>(device.deviceId),
                     property.name);
        activeReadDeviceIndex = -1;
        activeReadPropertyIndex = -1;
        requestNextBacnetProperty();
      }
      return;
    }
  }

  if (bacnetDevices[0].active && !bacnetDevices[0].valueScanFinished) {
    if (startNextValueScanProbe(0)) {
      return;
    }
  }

  if (bacnetDevices[0].active && bacnetDevices[0].valueScanFinished) {
    for (size_t i = 0; i < kMaxBacnetAnalogValues; ++i) {
      BacnetValueObjectPreview& object = bacnetDevices[0].analogValues[i];
      if (object.discovered &&
          millis() - object.lastPresentValueRefreshAt >=
              kBacnetPresentValueRefreshMs) {
        if (startValueObjectPresentValueRead(
                0, kBacnetAnalogValueObjectType, i,
                BacnetReadTarget::RefreshPresentValue)) {
          return;
        }
      }
    }

    for (size_t i = 0; i < kMaxBacnetMultiStateValues; ++i) {
      BacnetValueObjectPreview& object = bacnetDevices[0].multiStateValues[i];
      if (object.discovered &&
          millis() - object.lastPresentValueRefreshAt >=
              kBacnetPresentValueRefreshMs) {
        if (startValueObjectPresentValueRead(
                0, kBacnetMultiStateValueObjectType, i,
                BacnetReadTarget::RefreshPresentValue)) {
          return;
        }
      }
    }
  }
}

static void pollReadProperty() {
  if (activeReadTarget == BacnetReadTarget::None ||
      activeReadDeviceIndex < 0) {
    return;
  }

  BacnetDevicePreview& device = bacnetDevices[activeReadDeviceIndex];
  if ((activeReadTarget == BacnetReadTarget::DeviceProperty &&
       activeReadPropertyIndex < 0) ||
      activeReadTarget == BacnetReadTarget::None) {
    activeReadTarget = BacnetReadTarget::None;
    activeReadDeviceIndex = -1;
    activeReadPropertyIndex = -1;
    activeReadValueIndex = -1;
    return;
  }

  BacnetValue value;
  const BacnetReadPropertyPollStatus pollStatus =
      bacnetClient.pollReadPropertyStatus(value, activeReadInvokeId,
                                          activeReadRequest);
  if (pollStatus == BacnetReadPropertyPollStatus::Ack) {
    if (activeReadTarget == BacnetReadTarget::DeviceProperty) {
      BacnetPropertyPreview& property =
          device.properties[activeReadPropertyIndex];
      property.value = value.displayText();
      property.status = "ok";
      property.received = true;

      Serial.print("[I] BACnet ReadProperty ");
      Serial.print(property.name);
      Serial.print("=");
      Serial.println(property.value);
      bacnetGuiLog(LogLevel::Info,
                   "ReadProperty device,%lu %s = %s",
                   static_cast<unsigned long>(device.deviceId),
                   property.name, property.value.c_str());
    } else if (activeReadTarget == BacnetReadTarget::ScanObjectName) {
      size_t limit = 0;
      BacnetValueObjectPreview* objects =
          valueObjectsForType(device, activeReadObjectType, limit);
      if (objects != nullptr) {
        for (size_t i = 0; i < limit; ++i) {
          if (!objects[i].discovered) {
            objects[i].discovered = true;
            objects[i].object =
                BacnetObjectId{activeReadObjectType, activeReadObjectInstance};
            objects[i].objectName = value.displayText();
            objects[i].label = selectBacnetLabel(objects[i]);
            activeReadValueIndex = static_cast<int>(i);
            Serial.print("[I] BACnet found ");
            Serial.print(objectTypeName(activeReadObjectType));
            Serial.print(",");
            Serial.print(activeReadObjectInstance);
            Serial.print(" objectName=");
            Serial.println(objects[i].objectName);
            bacnetGuiLog(LogLevel::Info, "found %s,%lu objectName=%s",
                         objectTypeName(activeReadObjectType),
                         static_cast<unsigned long>(activeReadObjectInstance),
                         objects[i].objectName.c_str());
            if (!startValueObjectDescriptionRead(activeReadDeviceIndex,
                                                 activeReadObjectType, i) &&
                !startValueObjectPresentValueRead(
                    activeReadDeviceIndex, activeReadObjectType, i,
                    BacnetReadTarget::ScanPresentValue)) {
              clearActiveReadState();
              requestNextBacnetProperty();
            }
            return;
          }
        }
      }
    } else if (activeReadTarget == BacnetReadTarget::ScanDescription) {
      size_t limit = 0;
      BacnetValueObjectPreview* objects =
          valueObjectsForType(device, activeReadObjectType, limit);
      if (objects != nullptr && activeReadValueIndex >= 0 &&
          static_cast<size_t>(activeReadValueIndex) < limit) {
        BacnetValueObjectPreview& object = objects[activeReadValueIndex];
        object.description = value.displayText();
        object.label = selectBacnetLabel(object);
        if (!startValueObjectPresentValueRead(activeReadDeviceIndex,
                                              activeReadObjectType,
                                              activeReadValueIndex,
                                              BacnetReadTarget::ScanPresentValue)) {
          clearActiveReadState();
          requestNextBacnetProperty();
        }
        return;
      }
    } else if (activeReadTarget == BacnetReadTarget::ScanPresentValue ||
               activeReadTarget == BacnetReadTarget::RefreshPresentValue) {
      size_t limit = 0;
      BacnetValueObjectPreview* objects =
          valueObjectsForType(device, activeReadObjectType, limit);
      if (objects != nullptr && activeReadValueIndex >= 0 &&
          static_cast<size_t>(activeReadValueIndex) < limit) {
        BacnetValueObjectPreview& object = objects[activeReadValueIndex];
        object.presentValue = value.displayText();
        object.presentValueStatus = "ok";
        object.lastPresentValueRefreshAt = millis();
        object.label = selectBacnetLabel(object);

        Serial.print("[I] BACnet ReadProperty ");
        Serial.print(objectTypeName(activeReadObjectType));
        Serial.print(",");
        Serial.print(object.object.instance);
        Serial.print(" presentValue=");
        Serial.println(object.presentValue);
        bacnetGuiLog(LogLevel::Info,
                     "found %s,%lu objectName=%s description=%s label=%s "
                     "presentValue=%s",
                     objectTypeName(activeReadObjectType),
                     static_cast<unsigned long>(object.object.instance),
                     object.objectName.c_str(), object.description.c_str(),
                     object.label.c_str(), object.presentValue.c_str());
      }
    }

    activeReadTarget = BacnetReadTarget::None;
    activeReadDeviceIndex = -1;
    activeReadPropertyIndex = -1;
    activeReadValueIndex = -1;
    requestNextBacnetProperty();
    return;
  }

  if (pollStatus == BacnetReadPropertyPollStatus::Error) {
    const String status = String("failed: ") + value.displayText();
    if (activeReadTarget == BacnetReadTarget::DeviceProperty) {
      BacnetPropertyPreview& property =
          device.properties[activeReadPropertyIndex];
      property.failed = true;
      property.status = status;
      Serial.print("[W] BACnet ReadProperty ");
      Serial.print(property.name);
      Serial.print(" failed: ");
      Serial.println(value.displayText());
      bacnetGuiLog(LogLevel::Warn,
                   "ReadProperty device,%lu %s failed invoke %u: %s",
                   static_cast<unsigned long>(device.deviceId),
                   property.name, activeReadInvokeId, value.displayText());
    } else if (activeReadTarget == BacnetReadTarget::ScanDescription) {
      Serial.print("[W] BACnet ReadProperty ");
      Serial.print(objectTypeName(activeReadObjectType));
      Serial.print(",");
      Serial.print(activeReadObjectInstance);
      Serial.print(" description failed: ");
      Serial.println(value.displayText());
      bacnetGuiLog(LogLevel::Warn, "%s,%lu description failed: %s",
                   objectTypeName(activeReadObjectType),
                   static_cast<unsigned long>(activeReadObjectInstance),
                   value.displayText());
      if (!startValueObjectPresentValueRead(
              activeReadDeviceIndex, activeReadObjectType,
              activeReadValueIndex, BacnetReadTarget::ScanPresentValue)) {
        clearActiveReadState();
        requestNextBacnetProperty();
      }
      return;
    } else if (activeReadTarget == BacnetReadTarget::ScanPresentValue ||
               activeReadTarget == BacnetReadTarget::RefreshPresentValue) {
      size_t limit = 0;
      BacnetValueObjectPreview* objects =
          valueObjectsForType(device, activeReadObjectType, limit);
      if (objects != nullptr && activeReadValueIndex >= 0 &&
          static_cast<size_t>(activeReadValueIndex) < limit) {
        BacnetValueObjectPreview& object = objects[activeReadValueIndex];
        object.presentValueStatus = "read failed";
        object.lastPresentValueRefreshAt = millis();
      }
      Serial.print("[W] BACnet ReadProperty ");
      Serial.print(objectTypeName(activeReadObjectType));
      Serial.print(",");
      Serial.print(activeReadObjectInstance);
      Serial.print(" presentValue failed: ");
      Serial.println(value.displayText());
      bacnetGuiLog(LogLevel::Warn, "%s,%lu presentValue failed: %s",
                   objectTypeName(activeReadObjectType),
                   static_cast<unsigned long>(activeReadObjectInstance),
                   value.displayText());
    }

    activeReadTarget = BacnetReadTarget::None;
    activeReadDeviceIndex = -1;
    activeReadPropertyIndex = -1;
    activeReadValueIndex = -1;
    requestNextBacnetProperty();
    return;
  }

  if (millis() - activeReadStartedAt < activeReadTimeoutMs) {
    return;
  }

  if (activeReadTarget == BacnetReadTarget::DeviceProperty) {
    BacnetPropertyPreview& property =
        device.properties[activeReadPropertyIndex];
    property.failed = true;
    property.status = "timeout";
    Serial.print("[W] BACnet ReadProperty timeout ");
    Serial.println(property.name);
    bacnetGuiLog(LogLevel::Warn,
                 "ReadProperty device,%lu %s timeout invoke %u",
                 static_cast<unsigned long>(device.deviceId), property.name,
                 activeReadInvokeId);
  } else if (activeReadTarget == BacnetReadTarget::ScanDescription) {
    Serial.print("[W] BACnet ReadProperty timeout ");
    Serial.print(objectTypeName(activeReadObjectType));
    Serial.print(",");
    Serial.print(activeReadObjectInstance);
    Serial.println(" description");
    bacnetGuiLog(LogLevel::Warn, "%s,%lu description timeout invoke %u",
                 objectTypeName(activeReadObjectType),
                 static_cast<unsigned long>(activeReadObjectInstance),
                 activeReadInvokeId);
    if (!startValueObjectPresentValueRead(
            activeReadDeviceIndex, activeReadObjectType,
            activeReadValueIndex, BacnetReadTarget::ScanPresentValue)) {
      clearActiveReadState();
      requestNextBacnetProperty();
    }
    return;
  } else if (activeReadTarget == BacnetReadTarget::ScanPresentValue ||
             activeReadTarget == BacnetReadTarget::RefreshPresentValue) {
    size_t limit = 0;
    BacnetValueObjectPreview* objects =
        valueObjectsForType(device, activeReadObjectType, limit);
    if (objects != nullptr && activeReadValueIndex >= 0 &&
        static_cast<size_t>(activeReadValueIndex) < limit) {
      BacnetValueObjectPreview& object = objects[activeReadValueIndex];
      object.presentValueStatus = "read failed";
      object.lastPresentValueRefreshAt = millis();
    }
    Serial.print("[W] BACnet ReadProperty timeout ");
    Serial.print(objectTypeName(activeReadObjectType));
    Serial.print(",");
    Serial.println(activeReadObjectInstance);
    bacnetGuiLog(LogLevel::Warn, "%s,%lu presentValue timeout invoke %u",
                 objectTypeName(activeReadObjectType),
                 static_cast<unsigned long>(activeReadObjectInstance),
                 activeReadInvokeId);
  }

  activeReadTarget = BacnetReadTarget::None;
  activeReadDeviceIndex = -1;
  activeReadPropertyIndex = -1;
  activeReadValueIndex = -1;
  requestNextBacnetProperty();
}

static void pollBacnetDiscovery() {
  if (!bacnetStarted) {
    return;
  }

  pollReadProperty();

  BacnetIAmDevice device;
  if (bacnetClient.pollIAm(device)) {
    receivedIAmSinceLastWhoIs = true;
    Serial.println("[I] BACnet I-Am received");
    Serial.print("[I] Device ID: ");
    Serial.println(device.deviceInstance);
    Serial.print("[I] Max APDU: ");
    Serial.println(device.maxApduLengthAccepted);
    Serial.print("[I] Segmentation: ");
    Serial.println(device.segmentationSupported);
    Serial.print("[I] Vendor ID: ");
    Serial.println(device.vendorId);
    bacnetGuiLog(LogLevel::Info,
                 "I-Am device %lu from %s vendor %u",
                 static_cast<unsigned long>(device.deviceInstance),
                 kWagoAddress.toString().c_str(), device.vendorId);
    recordBacnetDevice(device);
    requestNextBacnetProperty();
  }

  if (millis() - lastWhoIsAt >= kWhoIsIntervalMs) {
    sendWhoIs();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("[I] BACnet client discovery hardware demo starting");

  ConfigManagerClass::setLogger([](const char* msg) {
    Serial.print("[D] CM ");
    Serial.println(msg);
  });

  ConfigManager.setAppName(APP_NAME);
  ConfigManager.setAppTitle(APP_NAME);
  ConfigManager.setVersion(VERSION);
  ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);
  ConfigManager.enableBuiltinSystemProvider();
  ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE,
                             sizeof(GLOBAL_THEME_OVERRIDE) - 1);
  setupGuiLogging();

  coreSettings.attachWiFi(ConfigManager);
  coreSettings.attachSystem(ConfigManager);
  coreSettings.attachNtp(ConfigManager);

  tempSettings.create();
  tempSettings.placeInUi();

  ConfigManager.loadAll();
  setupNetworkDefaults();

  setupRuntimeUI();
  setupTemperatureMeasuring();

#if defined(WIFI_FILTER_MAC_PRIORITY)
  ConfigManager.setAccessPointMacPriority(WIFI_FILTER_MAC_PRIORITY);
#endif

  ConfigManager.startWebServer();

  Serial.println("[I] Setup completed, starting main loop");
}

void onWiFiConnected() {
  wifiServices.onConnected(ConfigManager, APP_NAME, systemSettings, ntpSettings);
  Serial.print("[I] WiFi station IP: ");
  Serial.println(WiFi.localIP());
  startBacnetDiscovery();
}

void onWiFiDisconnected() {
  wifiServices.onDisconnected();
  bacnetClient.end();
  bacnetStarted = false;
  Serial.println("[W] WiFi disconnected");
}

void onWiFiAPMode() {
  wifiServices.onAPMode();
  Serial.print("[I] WiFi AP mode IP: ");
  Serial.println(WiFi.softAPIP());
}

static void setupNetworkDefaults() {
  if (wifiSettings.wifiSsid.get().isEmpty()) {
#if CM_HAS_WIFI_SECRETS
    Serial.println("[I] WiFi SSID empty, applying local secret defaults");
    wifiSettings.wifiSsid.set(MY_WIFI_SSID);
    wifiSettings.wifiPassword.set(MY_WIFI_PASSWORD);

#ifdef MY_WIFI_IP
    wifiSettings.staticIp.set(MY_WIFI_IP);
#endif
#ifdef MY_USE_DHCP
    wifiSettings.useDhcp.set(MY_USE_DHCP);
#endif
#ifdef MY_GATEWAY_IP
    wifiSettings.gateway.set(MY_GATEWAY_IP);
#endif
#ifdef MY_SUBNET_MASK
    wifiSettings.subnet.set(MY_SUBNET_MASK);
#endif
#ifdef MY_DNS_IP
    wifiSettings.dnsPrimary.set(MY_DNS_IP);
#endif
    ConfigManager.saveAll();
    Serial.println("[I] Restarting after applying WiFi defaults");
    delay(500);
    ESP.restart();
#else
    Serial.println("[W] WiFi SSID empty and secret/secrets.h missing");
#endif
  }

  if (systemSettings.otaPassword.get() != OTA_PASSWORD) {
    systemSettings.otaPassword.save(OTA_PASSWORD);
  }
}

void loop() {
  ConfigManager.getWiFiManager().update();
  ConfigManager.handleClient();
  cm::LoggingManager::instance().loop();
  alarmManager.update();
  pollBacnetDiscovery();

  static unsigned long lastLoopLog = 0;
  if (millis() - lastLoopLog > 60000) {
    lastLoopLog = millis();
    Serial.print("[D] Loop running, WiFi status=");
    Serial.print(WiFi.status());
    Serial.print(" heap=");
    Serial.println(ESP.getFreeHeap());
  }
}
