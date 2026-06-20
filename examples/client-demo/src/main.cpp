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
#include <cstdlib>
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
static constexpr uint32_t kBacnetPresentValueRefreshMs = 30000;
// WiFi-friendly BACnet demo scan tuning. Keep these bounded so the web UI and
// WiFi stack continue to run while enumerating Device object-list entries.
static constexpr uint32_t kBacnetDiscoveryWaitMs = 3000;
static constexpr uint32_t kReadPropertyTimeoutMs = 1000;
static constexpr uint32_t kBacnetObjectListReadTimeoutMs = 3000;
static constexpr uint32_t kBacnetScanReadTimeoutMs = kReadPropertyTimeoutMs;
static constexpr uint32_t kBacnetScanProbeDelayMs = 50;
static constexpr uint32_t kBacnetMaxObjectListEntriesToInspect = 600;
static constexpr uint32_t kBacnetMaxMissingObjectsInRow = 20;
static constexpr bool kBacnetEnableFallbackRangeScan = false;
// Fallback range probing is disabled by default and is only for local debug if
// a target cannot provide Device object-list data.
static constexpr uint32_t kAnalogValueScanStart = 200;
static constexpr uint32_t kMultiStateValueScanStart = 2000;
static constexpr uint32_t kMaxAnalogValueScanAttempts = 100;
static constexpr uint32_t kMaxMultiStateValueScanAttempts = 100;
static constexpr size_t kBacnetMaxFoundObjectsToDisplay = 10;
static constexpr size_t kMaxBacnetDevices = 1;
static constexpr size_t kMaxBacnetProperties = 5;
static constexpr size_t kMaxBacnetAnalogValues = kBacnetMaxFoundObjectsToDisplay;
static constexpr size_t kMaxBacnetMultiStateValues =
    kBacnetMaxFoundObjectsToDisplay;
static constexpr size_t kBacnetPreviewPropertyCount = 4;
static constexpr uint16_t kBacnetDeviceObjectType =
    static_cast<uint16_t>(BacnetObjectType::Device);
static constexpr uint16_t kBacnetAnalogValueObjectType =
    static_cast<uint16_t>(BacnetObjectType::AnalogValue);
static constexpr uint16_t kBacnetMultiStateValueObjectType =
    static_cast<uint16_t>(BacnetObjectType::MultiStateValue);
static constexpr uint32_t kAnalogValueScanEnd =
    kAnalogValueScanStart + kMaxAnalogValueScanAttempts - 1;
static constexpr uint32_t kMultiStateValueScanEnd =
    kMultiStateValueScanStart + kMaxMultiStateValueScanAttempts - 1;

static float temperature = 0.0f;
static float dewPoint = 0.0f;
static float humidity = 0.0f;
static float pressure = 0.0f;
static bool bacnetStarted = false;
static bool receivedIAmSinceLastWhoIs = true;
static bool discoveryWaitWarningLogged = false;
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
  bool objectListScanStarted = false;
  bool objectListScanFinished = false;
  bool objectListCountKnown = false;
  bool fallbackRangeScanStarted = false;
  bool analogScanFinished = false;
  bool multiStateScanFinished = false;
  uint32_t objectListCount = 0;
  uint32_t nextObjectListIndex = 1;
  uint32_t objectListEntriesInspected = 0;
  uint32_t nextAnalogValueInstance = kAnalogValueScanStart;
  uint32_t nextMultiStateValueInstance = kMultiStateValueScanStart;
  uint32_t analogMissingInRow = 0;
  uint32_t multiStateMissingInRow = 0;
  unsigned long lastScanProbeAt = 0;
};

enum class BacnetReadTarget {
  None,
  DeviceProperty,
  ScanObjectListCount,
  ScanObjectListEntry,
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
static uint32_t activeObjectListIndex = 0;
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

static LogLevel toCmLogLevel(BacnetLogLevel level) {
  switch (level) {
    case BacnetLogLevel::Fatal:
      return LogLevel::Fatal;
    case BacnetLogLevel::Error:
      return LogLevel::Error;
    case BacnetLogLevel::Warn:
      return LogLevel::Warn;
    case BacnetLogLevel::Debug:
      return LogLevel::Debug;
    case BacnetLogLevel::Trace:
      return LogLevel::Trace;
    case BacnetLogLevel::Info:
    case BacnetLogLevel::Off:
    default:
      return LogLevel::Info;
  }
}

static const char* bacnetLogLevelPrefix(BacnetLogLevel level) {
  switch (level) {
    case BacnetLogLevel::Fatal:
    case BacnetLogLevel::Error:
      return "[E]";
    case BacnetLogLevel::Warn:
      return "[W]";
    case BacnetLogLevel::Debug:
      return "[D]";
    case BacnetLogLevel::Trace:
      return "[T]";
    case BacnetLogLevel::Info:
    case BacnetLogLevel::Off:
    default:
      return "[I]";
  }
}

class ClientDemoBacnetLogOutput : public BacnetLogOutput {
 public:
  void log(const BacnetLogRecord& record) override {
    Serial.print(bacnetLogLevelPrefix(record.level));
    Serial.print(" ");
    Serial.print(record.tag != nullptr ? record.tag : "BACnet");
    Serial.print(" ");
    Serial.println(record.message != nullptr ? record.message : "");
    cm::LoggingManager::instance().logTag(
        toCmLogLevel(record.level), record.tag != nullptr ? record.tag : "BACnet",
        "%s", record.message != nullptr ? record.message : "");
  }
};

static ClientDemoBacnetLogOutput bacnetLogOutput;

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
  bacnetLogOutput.setLevel(BacnetLogLevel::Info);
  bacnetLogOutput.setTimestampMode(BacnetLogTimestampMode::Millis);
  bacnetLogOutput.setMinIntervalMs(0);
  bacnetClient.logger().addOutput(bacnetLogOutput);
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
  row += " - V:";
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
  discoveryWaitWarningLogged = false;
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
  if (isUsefulBacnetLabel(object.description, object.object.type,
                          object.object.instance)) {
    return object.description;
  }
  if (isUsefulBacnetLabel(object.objectName, object.object.type,
                          object.object.instance)) {
    return object.objectName;
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

static bool parseObjectIdentifierText(const char* text, BacnetObjectId& object) {
  if (text == nullptr || text[0] == '\0') {
    return false;
  }

  char* end = nullptr;
  const unsigned long objectType = std::strtoul(text, &end, 10);
  if (end == nullptr || *end != ',') {
    return false;
  }

  const unsigned long instance = std::strtoul(end + 1, &end, 10);
  if (objectType > UINT16_MAX || instance > 0x3FFFFF) {
    return false;
  }

  object.type = static_cast<uint16_t>(objectType);
  object.instance = static_cast<uint32_t>(instance);
  return true;
}

static bool objectIdFromValue(const BacnetValue& value, BacnetObjectId& object) {
  if (value.type == BacnetValueType::ObjectIdentifier) {
    object = value.objectValue;
    return true;
  }
  if (value.type == BacnetValueType::ObjectIdentifierList) {
    return parseObjectIdentifierText(value.displayText(), object);
  }
  return false;
}

static bool isScannedValueObjectType(uint16_t objectType) {
  return objectType == kBacnetAnalogValueObjectType ||
         objectType == kBacnetMultiStateValueObjectType;
}

static int findValueObjectIndex(BacnetDevicePreview& device,
                                BacnetObjectId object) {
  size_t limit = 0;
  BacnetValueObjectPreview* objects =
      valueObjectsForType(device, object.type, limit);
  if (objects == nullptr) {
    return -1;
  }

  for (size_t i = 0; i < limit; ++i) {
    if (objects[i].discovered &&
        objects[i].object.type == object.type &&
        objects[i].object.instance == object.instance) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

static int reserveValueObject(BacnetDevicePreview& device,
                              BacnetObjectId object) {
  const int existingIndex = findValueObjectIndex(device, object);
  if (existingIndex >= 0) {
    return existingIndex;
  }

  size_t limit = 0;
  BacnetValueObjectPreview* objects =
      valueObjectsForType(device, object.type, limit);
  if (objects == nullptr) {
    return -1;
  }

  for (size_t i = 0; i < limit; ++i) {
    if (!objects[i].discovered) {
      objects[i].discovered = true;
      objects[i].object = object;
      objects[i].objectName = "";
      objects[i].description = "";
      objects[i].label = "<unnamed>";
      objects[i].presentValue = "";
      objects[i].presentValueStatus = "pending";
      objects[i].lastPresentValueRefreshAt = 0;
      return static_cast<int>(i);
    }
  }
  return -1;
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

static const char* shortObjectTypeName(uint16_t objectType) {
  return objectType == kBacnetAnalogValueObjectType ? "AV" : "MV";
}

static uint32_t& missingCounterForType(BacnetDevicePreview& device,
                                       uint16_t objectType) {
  return objectType == kBacnetAnalogValueObjectType
             ? device.analogMissingInRow
             : device.multiStateMissingInRow;
}

static void markValueScanFinished(BacnetDevicePreview& device,
                                  uint16_t objectType,
                                  const char* reason) {
  const size_t foundCount = discoveredValueObjectCount(device, objectType);
  bacnetClient.logger().info(
      "BACnet/Scan",
      "scan stop %s found %u reason %s",
      objectTypeName(objectType), static_cast<unsigned>(foundCount),
      reason != nullptr ? reason : "complete");

  if (objectType == kBacnetAnalogValueObjectType) {
    device.analogScanFinished = true;
    if (foundCount == 0) {
      Serial.println("[W] BACnet scan found no analog-value objects");
      bacnetClient.logger().warn("BACnet/Scan",
                                 "Analog Values found: 0");
    }
  } else {
    device.multiStateScanFinished = true;
    if (foundCount == 0) {
      Serial.println("[W] BACnet scan found no multi-state-value objects");
      bacnetClient.logger().warn("BACnet/Scan",
                                 "Multi-State Values found: 0");
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
    bacnetClient.logger().info(
        "BACnet/Scan",
        "scan summary Analog Values found: %u, Multi-State Values found: %u",
        static_cast<unsigned>(avCount), static_cast<unsigned>(mvCount));
  }
}

static void resetMissingCounter(BacnetDevicePreview& device,
                                uint16_t objectType) {
  missingCounterForType(device, objectType) = 0;
}

static void recordMissingScanObject(BacnetDevicePreview& device,
                                    uint16_t objectType,
                                    uint32_t instance,
                                    const char* status) {
  uint32_t& missingInRow = missingCounterForType(device, objectType);
  ++missingInRow;
  bacnetClient.logger().info(
      "BACnet/Scan",
      "scan missing %s,%lu status %s missing-in-row %lu/%lu",
      objectTypeName(objectType), static_cast<unsigned long>(instance),
      status != nullptr ? status : "missing",
      static_cast<unsigned long>(missingInRow),
      static_cast<unsigned long>(kBacnetMaxMissingObjectsInRow));
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
  activeObjectListIndex = 0;
  activeReadRequest = BacnetPropertyRequest{};
  activeReadTimeoutMs = kReadPropertyTimeoutMs;
}

static bool startObjectListCountRead(size_t deviceIndex) {
  BacnetDevicePreview& device = bacnetDevices[deviceIndex];
  activeReadDeviceIndex = static_cast<int>(deviceIndex);
  activeReadPropertyIndex = -1;
  activeReadValueIndex = -1;
  activeReadObjectType = kBacnetDeviceObjectType;
  activeReadObjectInstance = device.deviceId;
  activeObjectListIndex = 0;

  const BacnetPropertyRequest request{
      BacnetObjectId{kBacnetDeviceObjectType, device.deviceId},
      BacnetPropertyId::ObjectList,
      0};
  bacnetClient.logger().info(
      "BACnet/Scan", "read device,%lu objectList[0] start",
      static_cast<unsigned long>(device.deviceId));
  return sendReadPropertyRequest(device, request,
                                 BacnetReadTarget::ScanObjectListCount,
                                 kBacnetObjectListReadTimeoutMs);
}

static bool startObjectListEntryRead(size_t deviceIndex) {
  BacnetDevicePreview& device = bacnetDevices[deviceIndex];
  if (device.objectListEntriesInspected >=
      kBacnetMaxObjectListEntriesToInspect) {
    return false;
  }
  if (device.objectListCountKnown &&
      device.nextObjectListIndex > device.objectListCount) {
    return false;
  }

  const uint32_t objectListIndex = device.nextObjectListIndex++;
  ++device.objectListEntriesInspected;
  activeReadDeviceIndex = static_cast<int>(deviceIndex);
  activeReadPropertyIndex = -1;
  activeReadValueIndex = -1;
  activeReadObjectType = kBacnetDeviceObjectType;
  activeReadObjectInstance = device.deviceId;
  activeObjectListIndex = objectListIndex;

  const BacnetPropertyRequest request{
      BacnetObjectId{kBacnetDeviceObjectType, device.deviceId},
      BacnetPropertyId::ObjectList,
      objectListIndex};
  bacnetClient.logger().info(
      "BACnet/Scan", "read device,%lu objectList[%lu] start",
      static_cast<unsigned long>(device.deviceId),
      static_cast<unsigned long>(objectListIndex));
  return sendReadPropertyRequest(device, request,
                                 BacnetReadTarget::ScanObjectListEntry,
                                 kBacnetObjectListReadTimeoutMs);
}

static bool startValueObjectNameRead(size_t deviceIndex, uint16_t objectType,
                                     size_t valueIndex) {
  BacnetDevicePreview& device = bacnetDevices[deviceIndex];
  size_t limit = 0;
  BacnetValueObjectPreview* objects =
      valueObjectsForType(device, objectType, limit);
  if (objects == nullptr || valueIndex >= limit ||
      !objects[valueIndex].discovered) {
    return false;
  }

  activeReadDeviceIndex = static_cast<int>(deviceIndex);
  activeReadValueIndex = static_cast<int>(valueIndex);
  activeReadPropertyIndex = -1;
  activeReadObjectType = objectType;
  activeReadObjectInstance = objects[valueIndex].object.instance;

  const BacnetPropertyRequest request{objects[valueIndex].object,
                                      BacnetPropertyId::ObjectName};
  bacnetClient.logger().info(
      "BACnet/Scan", "scan read attempt %s%lu objectName",
      shortObjectTypeName(objectType),
      static_cast<unsigned long>(objects[valueIndex].object.instance));
  return sendReadPropertyRequest(device, request,
                                 BacnetReadTarget::ScanObjectName,
                                 kReadPropertyTimeoutMs);
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
  bacnetClient.logger().info(
      "BACnet/Scan",
      "scan read attempt %s,%lu objectName invoke %u",
      objectTypeName(objectType), static_cast<unsigned long>(instance),
      static_cast<unsigned>(activeReadInvokeId));
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
    bacnetClient.logger().info(
        "BACnet/Scan", "scan start device %lu at %s",
        static_cast<unsigned long>(device.deviceId),
        device.address.toString().c_str());
    bacnetClient.logger().info(
        "BACnet/Scan",
        "scan config discoveryWait=%lums objectListTimeout=%lums "
        "readTimeout=%lums delay=%lums maxObjectList=%lu maxDisplay=%u "
        "fallbackRange=%s",
        static_cast<unsigned long>(kBacnetDiscoveryWaitMs),
        static_cast<unsigned long>(kBacnetObjectListReadTimeoutMs),
        static_cast<unsigned long>(kReadPropertyTimeoutMs),
        static_cast<unsigned long>(kBacnetScanProbeDelayMs),
        static_cast<unsigned long>(kBacnetMaxObjectListEntriesToInspect),
        static_cast<unsigned>(kBacnetMaxFoundObjectsToDisplay),
        kBacnetEnableFallbackRangeScan ? "enabled" : "disabled");
    if (kBacnetEnableFallbackRangeScan) {
      bacnetClient.logger().info(
          "BACnet/Scan",
          "fallback config missingLimit=%lu analog-value %lu..%lu "
          "multi-state-value %lu..%lu",
          static_cast<unsigned long>(kBacnetMaxMissingObjectsInRow),
          static_cast<unsigned long>(kAnalogValueScanStart),
          static_cast<unsigned long>(kAnalogValueScanEnd),
          static_cast<unsigned long>(kMultiStateValueScanStart),
          static_cast<unsigned long>(kMultiStateValueScanEnd));
    }
  }

  if (!device.objectListScanStarted) {
    device.objectListScanStarted = true;
    if (startObjectListCountRead(deviceIndex)) {
      return true;
    }
    device.objectListScanFinished = true;
    bacnetClient.logger().warn(
        "BACnet/Scan", "objectList count read could not be sent");
  }

  if (!device.objectListScanFinished) {
    const bool displayLimitReached =
        discoveredValueObjectCount(device, kBacnetAnalogValueObjectType) >=
            kMaxBacnetAnalogValues &&
        discoveredValueObjectCount(device, kBacnetMultiStateValueObjectType) >=
            kMaxBacnetMultiStateValues;
    const bool countFinished = device.objectListCountKnown &&
                               device.nextObjectListIndex >
                                   device.objectListCount;
    const bool inspectLimitReached =
        device.objectListEntriesInspected >=
        kBacnetMaxObjectListEntriesToInspect;
    if (displayLimitReached || countFinished || inspectLimitReached) {
      device.objectListScanFinished = true;
      bacnetClient.logger().info(
          "BACnet/Scan",
          "objectList scan finished inspected=%lu count=%lu reason %s",
          static_cast<unsigned long>(device.objectListEntriesInspected),
          static_cast<unsigned long>(device.objectListCount),
          displayLimitReached
              ? "display limit reached"
              : (inspectLimitReached ? "inspect limit reached"
                                     : "object-list complete"));
      if (!kBacnetEnableFallbackRangeScan) {
        markValueScanFinished(device, kBacnetAnalogValueObjectType,
                              "object-list complete");
        markValueScanFinished(device, kBacnetMultiStateValueObjectType,
                              "object-list complete");
      }
      return false;
    }

    if (millis() - device.lastScanProbeAt < kBacnetScanProbeDelayMs) {
      return false;
    }

    device.lastScanProbeAt = millis();
    return startObjectListEntryRead(deviceIndex);
  }

  if (!kBacnetEnableFallbackRangeScan) {
    if (!device.analogScanFinished) {
      markValueScanFinished(device, kBacnetAnalogValueObjectType,
                            "object-list complete");
    }
    if (!device.multiStateScanFinished) {
      markValueScanFinished(device, kBacnetMultiStateValueObjectType,
                            "object-list complete");
    }
    return false;
  }

  if (!device.fallbackRangeScanStarted) {
    device.fallbackRangeScanStarted = true;
    bacnetClient.logger().warn("BACnet/Scan",
                               "objectList unavailable, fallback range scan "
                               "enabled");
  }

  if (millis() - device.lastScanProbeAt < kBacnetScanProbeDelayMs) {
    return false;
  }

  if (!device.analogScanFinished) {
    if (discoveredValueObjectCount(device, kBacnetAnalogValueObjectType) >=
            kMaxBacnetAnalogValues) {
      markValueScanFinished(device, kBacnetAnalogValueObjectType,
                            "display limit reached");
      return false;
    }
    if (device.analogMissingInRow >= kBacnetMaxMissingObjectsInRow) {
      markValueScanFinished(device, kBacnetAnalogValueObjectType,
                            "missing limit reached");
      return false;
    }
    if (device.nextAnalogValueInstance > kAnalogValueScanEnd) {
      markValueScanFinished(device, kBacnetAnalogValueObjectType,
                            "attempt limit reached");
      return false;
    }

    device.lastScanProbeAt = millis();
    return startValueObjectProbe(deviceIndex, kBacnetAnalogValueObjectType,
                                 device.nextAnalogValueInstance++);
  }

  if (!device.multiStateScanFinished) {
    if (discoveredValueObjectCount(device, kBacnetMultiStateValueObjectType) >=
            kMaxBacnetMultiStateValues) {
      markValueScanFinished(device, kBacnetMultiStateValueObjectType,
                            "display limit reached");
      return false;
    }
    if (device.multiStateMissingInRow >= kBacnetMaxMissingObjectsInRow) {
      markValueScanFinished(device, kBacnetMultiStateValueObjectType,
                            "missing limit reached");
      return false;
    }
    if (device.nextMultiStateValueInstance > kMultiStateValueScanEnd) {
      markValueScanFinished(device, kBacnetMultiStateValueObjectType,
                            "attempt limit reached");
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
    } else if (activeReadTarget == BacnetReadTarget::ScanObjectListCount) {
      if (value.type == BacnetValueType::Unsigned) {
        device.objectListCount = value.unsignedValue;
        device.objectListCountKnown = true;
        device.nextObjectListIndex = 1;
        device.objectListEntriesInspected = 0;
        const uint32_t effectiveObjectListLimit =
            device.objectListCount < kBacnetMaxObjectListEntriesToInspect
                ? device.objectListCount
                : kBacnetMaxObjectListEntriesToInspect;
        bacnetClient.logger().info(
            "BACnet/Scan",
            "objectList count = %lu inspect max %lu effective limit %lu",
            static_cast<unsigned long>(device.objectListCount),
            static_cast<unsigned long>(kBacnetMaxObjectListEntriesToInspect),
            static_cast<unsigned long>(effectiveObjectListLimit));
      } else {
        device.objectListScanFinished = true;
        bacnetClient.logger().warn(
            "BACnet/Scan", "objectList count unsupported value %s",
            value.displayText());
      }
    } else if (activeReadTarget == BacnetReadTarget::ScanObjectListEntry) {
      BacnetObjectId listedObject;
      if (objectIdFromValue(value, listedObject)) {
        bacnetClient.logger().info(
            "BACnet/Scan", "objectList[%lu] = %u,%lu",
            static_cast<unsigned long>(activeObjectListIndex),
            static_cast<unsigned>(listedObject.type),
            static_cast<unsigned long>(listedObject.instance));
        if (!isScannedValueObjectType(listedObject.type)) {
          bacnetClient.logger().info(
              "BACnet/Scan", "objectList[%lu] skipped object type %u",
              static_cast<unsigned long>(activeObjectListIndex),
              static_cast<unsigned>(listedObject.type));
        } else {
          const int valueIndex = reserveValueObject(device, listedObject);
          if (valueIndex >= 0) {
            bacnetClient.logger().info(
                "BACnet/Scan", "objectList[%lu] accepted %s%lu",
                static_cast<unsigned long>(activeObjectListIndex),
                shortObjectTypeName(listedObject.type),
                static_cast<unsigned long>(listedObject.instance));
            if (startValueObjectNameRead(activeReadDeviceIndex,
                                         listedObject.type,
                                         static_cast<size_t>(valueIndex))) {
              return;
            }
          } else {
            bacnetClient.logger().info(
                "BACnet/Scan", "objectList[%lu] skipped %s%lu display full",
                static_cast<unsigned long>(activeObjectListIndex),
                shortObjectTypeName(listedObject.type),
                static_cast<unsigned long>(listedObject.instance));
          }
        }
      } else {
        bacnetClient.logger().warn(
            "BACnet/Scan", "objectList[%lu] malformed value %s",
            static_cast<unsigned long>(activeObjectListIndex),
            value.displayText());
      }
    } else if (activeReadTarget == BacnetReadTarget::ScanObjectName) {
      size_t limit = 0;
      BacnetValueObjectPreview* objects =
          valueObjectsForType(device, activeReadObjectType, limit);
      if (objects != nullptr && activeReadValueIndex >= 0 &&
          static_cast<size_t>(activeReadValueIndex) < limit &&
          objects[activeReadValueIndex].discovered) {
        BacnetValueObjectPreview& object = objects[activeReadValueIndex];
        object.objectName = value.displayText();
        object.label = selectBacnetLabel(object);
        bacnetClient.logger().info(
            "BACnet/Scan", "scan read %s%lu objectName=%s",
            shortObjectTypeName(activeReadObjectType),
            static_cast<unsigned long>(object.object.instance),
            object.objectName.c_str());
        if (!startValueObjectDescriptionRead(activeReadDeviceIndex,
                                             activeReadObjectType,
                                             activeReadValueIndex) &&
            !startValueObjectPresentValueRead(
                activeReadDeviceIndex, activeReadObjectType,
                activeReadValueIndex, BacnetReadTarget::ScanPresentValue)) {
          clearActiveReadState();
          requestNextBacnetProperty();
        }
        return;
      }
      if (objects != nullptr) {
        for (size_t i = 0; i < limit; ++i) {
          if (!objects[i].discovered) {
            objects[i].discovered = true;
            objects[i].object =
                BacnetObjectId{activeReadObjectType, activeReadObjectInstance};
            objects[i].objectName = value.displayText();
            objects[i].label = selectBacnetLabel(objects[i]);
            resetMissingCounter(device, activeReadObjectType);
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
            bacnetClient.logger().info(
                "BACnet/Scan", "scan found %s%lu objectName=%s",
                shortObjectTypeName(activeReadObjectType),
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
        bacnetClient.logger().info(
            "BACnet/Scan", "scan read %s%lu description=%s",
            shortObjectTypeName(activeReadObjectType),
            static_cast<unsigned long>(object.object.instance),
            object.description.c_str());
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
        bacnetClient.logger().info(
            "BACnet/Scan", "scan value %s%lu label=%s presentValue=%s",
            shortObjectTypeName(activeReadObjectType),
            static_cast<unsigned long>(object.object.instance),
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
    } else if (activeReadTarget == BacnetReadTarget::ScanObjectListCount) {
      device.objectListScanFinished = true;
      bacnetClient.logger().warn(
          "BACnet/Scan", "objectList count read failed: %s",
          value.displayText());
    } else if (activeReadTarget == BacnetReadTarget::ScanObjectListEntry) {
      bacnetClient.logger().warn(
          "BACnet/Scan", "objectList[%lu] read failed: %s",
          static_cast<unsigned long>(activeObjectListIndex),
          value.displayText());
    } else if (activeReadTarget == BacnetReadTarget::ScanObjectName) {
      if (activeReadValueIndex >= 0) {
        bacnetClient.logger().warn(
            "BACnet/Scan", "scan read %s%lu objectName failed: %s",
            shortObjectTypeName(activeReadObjectType),
            static_cast<unsigned long>(activeReadObjectInstance),
            value.displayText());
        if (!startValueObjectDescriptionRead(
                activeReadDeviceIndex, activeReadObjectType,
                activeReadValueIndex) &&
            !startValueObjectPresentValueRead(
                activeReadDeviceIndex, activeReadObjectType,
                activeReadValueIndex, BacnetReadTarget::ScanPresentValue)) {
          clearActiveReadState();
          requestNextBacnetProperty();
        }
        return;
      } else {
        Serial.print("[I] BACnet scan missing ");
        Serial.print(objectTypeName(activeReadObjectType));
        Serial.print(",");
        Serial.print(activeReadObjectInstance);
        Serial.print(" objectName: ");
        Serial.println(value.displayText());
        recordMissingScanObject(device, activeReadObjectType,
                                activeReadObjectInstance,
                                value.displayText());
      }
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
      bacnetClient.logger().warn(
          "BACnet/Scan", "scan read %s%lu description failed: %s",
          shortObjectTypeName(activeReadObjectType),
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
      bacnetClient.logger().warn(
          "BACnet/Scan", "scan read %s%lu presentValue failed: %s",
          shortObjectTypeName(activeReadObjectType),
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

  bacnetClient.logReadPropertyTimeout(activeReadInvokeId, activeReadRequest);

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
  } else if (activeReadTarget == BacnetReadTarget::ScanObjectListCount) {
    device.objectListScanFinished = true;
    bacnetClient.logger().warn(
        "BACnet/Scan", "objectList count timeout invoke %u",
        activeReadInvokeId);
  } else if (activeReadTarget == BacnetReadTarget::ScanObjectListEntry) {
    bacnetClient.logger().warn(
        "BACnet/Scan", "objectList[%lu] timeout invoke %u",
        static_cast<unsigned long>(activeObjectListIndex),
        activeReadInvokeId);
  } else if (activeReadTarget == BacnetReadTarget::ScanObjectName) {
    if (activeReadValueIndex >= 0) {
      bacnetClient.logger().warn(
          "BACnet/Scan", "scan read %s%lu objectName timeout invoke %u",
          shortObjectTypeName(activeReadObjectType),
          static_cast<unsigned long>(activeReadObjectInstance),
          activeReadInvokeId);
      if (!startValueObjectDescriptionRead(
              activeReadDeviceIndex, activeReadObjectType,
              activeReadValueIndex) &&
          !startValueObjectPresentValueRead(
              activeReadDeviceIndex, activeReadObjectType,
              activeReadValueIndex, BacnetReadTarget::ScanPresentValue)) {
        clearActiveReadState();
        requestNextBacnetProperty();
      }
      return;
    } else {
      Serial.print("[I] BACnet scan missing ");
      Serial.print(objectTypeName(activeReadObjectType));
      Serial.print(",");
      Serial.print(activeReadObjectInstance);
      Serial.println(" objectName timeout");
      recordMissingScanObject(device, activeReadObjectType,
                              activeReadObjectInstance, "timeout");
    }
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
    bacnetClient.logger().warn(
        "BACnet/Scan", "scan read %s%lu description timeout invoke %u",
        shortObjectTypeName(activeReadObjectType),
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
    bacnetClient.logger().warn(
        "BACnet/Scan", "scan read %s%lu presentValue timeout invoke %u",
        shortObjectTypeName(activeReadObjectType),
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
    discoveryWaitWarningLogged = false;
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

  if (!receivedIAmSinceLastWhoIs && !discoveryWaitWarningLogged &&
      millis() - lastWhoIsAt >= kBacnetDiscoveryWaitMs) {
    discoveryWaitWarningLogged = true;
    Serial.println("[W] BACnet discovery wait timed out");
    bacnetClient.logger().warn(
        "BACnet/Discovery", "no I-Am received within %lums",
        static_cast<unsigned long>(kBacnetDiscoveryWaitMs));
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
