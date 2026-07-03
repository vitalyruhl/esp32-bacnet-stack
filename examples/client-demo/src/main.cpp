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

#ifndef APP_VERSION
#define APP_VERSION "0.20.0"
#endif
#ifndef APP_NAME
#define APP_NAME "BACnet Client Demo"
#endif

#ifndef SETTINGS_PASSWORD
#define SETTINGS_PASSWORD ""
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD SETTINGS_PASSWORD
#endif

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

#ifndef BACNET_WHOIS_DESTINATION
#define BACNET_WHOIS_DESTINATION "192.0.2.255"
#endif

#ifndef BACNET_TARGET_ADDRESS
#define BACNET_TARGET_ADDRESS "192.0.2.101"
#endif

#ifndef BACNET_TARGET_DEVICE_INSTANCE
#define BACNET_TARGET_DEVICE_INSTANCE 9001
#endif

#ifndef BACNET_TARGET_PORT
#define BACNET_TARGET_PORT BacnetClient::kDefaultPort
#endif

#ifndef BACNET_STATUS_OBJECT_TYPE
#define BACNET_STATUS_OBJECT_TYPE 0
#endif

#ifndef BACNET_STATUS_OBJECT_INSTANCE
#define BACNET_STATUS_OBJECT_INSTANCE 0
#endif

#ifndef BACNET_FALLBACK_ANALOG_OBJECT_TYPE
#define BACNET_FALLBACK_ANALOG_OBJECT_TYPE 2
#endif
#ifndef BACNET_FALLBACK_ANALOG_OBJECT_INSTANCE
#define BACNET_FALLBACK_ANALOG_OBJECT_INSTANCE 220
#endif
#ifndef BACNET_FALLBACK_BINARY_OBJECT_TYPE
#define BACNET_FALLBACK_BINARY_OBJECT_TYPE 5
#endif
#ifndef BACNET_FALLBACK_BINARY_OBJECT_INSTANCE
#define BACNET_FALLBACK_BINARY_OBJECT_INSTANCE 320
#endif
#ifndef BACNET_FALLBACK_MULTISTATE_OBJECT_TYPE
#define BACNET_FALLBACK_MULTISTATE_OBJECT_TYPE 19
#endif
#ifndef BACNET_FALLBACK_MULTISTATE_OBJECT_INSTANCE
#define BACNET_FALLBACK_MULTISTATE_OBJECT_INSTANCE 2020
#endif

static constexpr uint32_t kWhoIsIntervalMs = 30000;
static constexpr uint32_t kBacnetScanReadTimeoutMs = 3000;
static constexpr uint32_t kBacnetMaxObjectListEntriesToInspect = 600;
static constexpr size_t kBacnetMaxFoundObjectsToDisplay = 10;
static constexpr size_t kBacnetScanResultCapacity =
  kBacnetMaxFoundObjectsToDisplay * 3;
static constexpr size_t kBacnetPreviewPropertyCount = 4;
static constexpr uint32_t kBacnetSubscriptionFallbackPollMs = 30000;

static float temperature = 0.0f;
static float dewPoint = 0.0f;
static float humidity = 0.0f;
static float pressure = 0.0f;

static IPAddress whoIsDestination;
static IPAddress configuredBacnetTargetAddress;
static bool bacnetAddressConfigValid = false;
static bool bacnetAddressConfigErrorLogged = false;
static bool bacnetStarted = false;
static bool bacnetScanFinished = false;
static bool bacnetScanRequested = false;
static bool bacnetScanRunning = false;
static bool bacnetDeviceSelected = false;
static bool bacnetRescanPending = false;
static uint16_t activeBacnetVendorId = 0;
static String bacnetScanStatus = "Scan not started";
static String bacnetRescanSource = "";
static size_t bacnetScanStoredObjects = 0;
static BacnetObjectListScanPhase lastLoggedScanPhase =
    BacnetObjectListScanPhase::Idle;
static uint32_t lastLoggedScanIndex = 0;
static unsigned long lastWhoIsAt = 0;

struct BacnetPropertySpec {
  const char* name = "";
  BacnetPropertyId id = BacnetPropertyId::ObjectName;
};

struct BacnetPropertyPreview {
  const char* name = "";
  BacnetPropertyId id = BacnetPropertyId::ObjectName;
  String value = "not read";
  BacnetDeviceSessionReadStatus status = BacnetDeviceSessionReadStatus::Skipped;
};

struct BacnetValueObjectPreview {
  bool discovered = false;
  BacnetObjectId object;
  String objectName;
  String description;
  String label;
  String presentValue = "";
  String presentValueStatus = "pending";
  std::unique_ptr<BacnetPropertySubscription> subscription;
};

struct BacnetObjectStatusPreview {
  bool available = false;
  BacnetObjectId object;
  String label = "not read";
  String presentValue = "not read";
  String presentValueStatus = "skipped";
  String state = "Unknown";
  String flags = "skipped";
  String eventState = "skipped";
  String reliability = "skipped";
  String outOfService = "skipped";
};

static constexpr BacnetPropertySpec
    kBacnetPreviewProperties[kBacnetPreviewPropertyCount] = {
        {"objectName", BacnetPropertyId::ObjectName},
        {"vendorName", BacnetPropertyId::VendorName},
        {"modelName", BacnetPropertyId::ModelName},
        {"firmwareRevision", BacnetPropertyId::FirmwareRevision},
};

static BacnetPropertyPreview bacnetDeviceProperties[kBacnetPreviewPropertyCount];
static BacnetValueObjectPreview analogValues[kBacnetMaxFoundObjectsToDisplay];
static BacnetValueObjectPreview binaryValues[kBacnetMaxFoundObjectsToDisplay];
static BacnetValueObjectPreview multiStateValues[kBacnetMaxFoundObjectsToDisplay];
static BacnetObjectStatusPreview bacnetStatusPreview;
static BacnetScannedObject scanBuffer[kBacnetScanResultCapacity];
static BacnetObjectListScanJob scanJob;
static std::unique_ptr<BacnetDeviceSession> activeBacnetSession;
static const BacnetObjectType kValueObjectScanFilter[] = {
  BacnetObjectType::AnalogInput,
  BacnetObjectType::AnalogOutput,
    BacnetObjectType::AnalogValue,
  BacnetObjectType::BinaryInput,
  BacnetObjectType::BinaryOutput,
  BacnetObjectType::BinaryValue,
  BacnetObjectType::MultiStateInput,
  BacnetObjectType::MultiStateOutput,
    BacnetObjectType::MultiStateValue,
};

static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
.myCSSTempClass { color:rgb(198, 16, 16) !important; font-weight:900!important; font-size: 1.2rem!important; }
.live-cards {
  align-items: flex-start !important;
  grid-template-columns: minmax(620px, 820px) minmax(280px, 1fr) !important;
}

.live-cards > .card:first-child {
  flex: 1 1 680px !important;
  min-width: 620px !important;
  max-width: 820px !important;
}

.live-cards > .card:first-child .rw {
  display: grid !important;
  grid-template-columns: 140px minmax(0, 1fr) auto !important;
  column-gap: 12px !important;
  align-items: start !important;
}

.dv[data-label="Analog Values"] + .rw .val,
.dv[data-label="Binary Values"] + .rw .val,
.dv[data-label="Multi-State Values"] + .rw .val,
.dv[data-label="Object Health"] + .rw .val {
  display: block !important;
  text-align: left !important;
  white-space: pre-line !important;
  overflow-wrap: anywhere !important;
  line-height: 1.35 !important;
  font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", monospace !important;
  font-size: 0.88rem !important;
}
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
  auto guiOut =
      std::make_unique<cm::LoggingManager::GuiOutput>(ConfigManager, 80);
  guiOut->setLevel(LogLevel::Info);
  guiOut->setMaxQueue(120);
  guiOut->setMaxPerTick(8);
  guiOut->addTimestamp(cm::LoggingManager::Output::TimestampMode::Millis);
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

static bool parseBacnetAddress(const char* label,
                               const char* addressText,
                               IPAddress& output) {
  if (addressText != nullptr && output.fromString(addressText)) {
    return true;
  }

  Serial.print("[E] Invalid BACnet ");
  Serial.print(label);
  Serial.print(" IP: ");
  Serial.println(addressText != nullptr ? addressText : "<null>");
  bacnetGuiLog(LogLevel::Error, "invalid BACnet %s IP: %s", label,
               addressText != nullptr ? addressText : "<null>");
  return false;
}

static bool configureBacnetAddresses() {
  bacnetAddressConfigValid =
      parseBacnetAddress("Who-Is destination", BACNET_WHOIS_DESTINATION,
                         whoIsDestination) &&
      parseBacnetAddress("target address", BACNET_TARGET_ADDRESS,
                         configuredBacnetTargetAddress);

  if (bacnetAddressConfigValid) {
    bacnetAddressConfigErrorLogged = false;
  }
  return bacnetAddressConfigValid;
}

static bool isZeroBacnetAddress(const IPAddress& address) {
  return address[0] == 0 && address[1] == 0 && address[2] == 0 &&
         address[3] == 0;
}

static String shortenBacnetLabel(const String& label) {
  static constexpr size_t kMaxLabelLength = 36;
  if (label.length() <= kMaxLabelLength) {
    return label;
  }
  return label.substring(0, kMaxLabelLength - 3) + "...";
}

static String valueTextIfAck(BacnetDeviceSessionReadStatus status,
                             const BacnetValue& value) {
  if (status == BacnetDeviceSessionReadStatus::Ack) {
    return value.displayText();
  }
  return bacnetReadStatusText(status);
}

static String propertyValueTextIfAck(BacnetPropertyReadStatus status,
                                     const BacnetValue& value) {
  if (status == BacnetPropertyReadStatus::Ack) {
    return value.displayText();
  }
  return bacnetPropertyReadStatusText(status);
}

static String objectTypePrefix(uint16_t objectType) {
  if (objectType == static_cast<uint16_t>(BacnetObjectType::AnalogInput)) {
    return "AI";
  }
  if (objectType == static_cast<uint16_t>(BacnetObjectType::AnalogOutput)) {
    return "AO";
  }
  if (objectType == static_cast<uint16_t>(BacnetObjectType::AnalogValue)) {
    return "AV";
  }
  if (objectType == static_cast<uint16_t>(BacnetObjectType::BinaryInput)) {
    return "BI";
  }
  if (objectType == static_cast<uint16_t>(BacnetObjectType::BinaryOutput)) {
    return "BO";
  }
  if (objectType == static_cast<uint16_t>(BacnetObjectType::BinaryValue)) {
    return "BV";
  }
  if (objectType == static_cast<uint16_t>(BacnetObjectType::MultiStateInput)) {
    return "MI";
  }
  if (objectType == static_cast<uint16_t>(BacnetObjectType::MultiStateOutput)) {
    return "MO";
  }
  if (objectType == static_cast<uint16_t>(BacnetObjectType::MultiStateValue)) {
    return "MV";
  }
  return "OBJ";
}

static bool isAnalogProcessObject(uint16_t objectType) {
  return objectType == static_cast<uint16_t>(BacnetObjectType::AnalogInput) ||
         objectType == static_cast<uint16_t>(BacnetObjectType::AnalogOutput) ||
         objectType == static_cast<uint16_t>(BacnetObjectType::AnalogValue);
}

static bool isBinaryProcessObject(uint16_t objectType) {
  return objectType == static_cast<uint16_t>(BacnetObjectType::BinaryInput) ||
         objectType == static_cast<uint16_t>(BacnetObjectType::BinaryOutput) ||
         objectType == static_cast<uint16_t>(BacnetObjectType::BinaryValue);
}

static bool isMultiStateProcessObject(uint16_t objectType) {
  return objectType ==
             static_cast<uint16_t>(BacnetObjectType::MultiStateInput) ||
         objectType ==
             static_cast<uint16_t>(BacnetObjectType::MultiStateOutput) ||
         objectType ==
             static_cast<uint16_t>(BacnetObjectType::MultiStateValue);
}

static String objectLabel(const BacnetScannedObject& scanned) {
  const String objectName =
      valueTextIfAck(scanned.objectNameStatus, scanned.objectName);
  if (scanned.objectNameStatus == BacnetDeviceSessionReadStatus::Ack &&
      objectName.length() > 0) {
    return objectName;
  }

  const String description =
      valueTextIfAck(scanned.descriptionStatus, scanned.description);
  if (scanned.descriptionStatus == BacnetDeviceSessionReadStatus::Ack &&
      description.length() > 0) {
    return description;
  }

  return objectTypePrefix(scanned.objectId.type) +
         String(scanned.objectId.instance);
}

static void resetValueObjects(BacnetValueObjectPreview* objects, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    objects[i].subscription.reset();
    objects[i] = BacnetValueObjectPreview{};
  }
}

static void resetBacnetPreviews() {
  for (size_t i = 0; i < kBacnetPreviewPropertyCount; ++i) {
    bacnetDeviceProperties[i].name = kBacnetPreviewProperties[i].name;
    bacnetDeviceProperties[i].id = kBacnetPreviewProperties[i].id;
    bacnetDeviceProperties[i].value = "not read";
    bacnetDeviceProperties[i].status = BacnetDeviceSessionReadStatus::Skipped;
  }
  resetValueObjects(analogValues, kBacnetMaxFoundObjectsToDisplay);
  resetValueObjects(binaryValues, kBacnetMaxFoundObjectsToDisplay);
  resetValueObjects(multiStateValues, kBacnetMaxFoundObjectsToDisplay);
  bacnetStatusPreview = BacnetObjectStatusPreview{};
  activeBacnetVendorId = 0;
  bacnetScanStatus = "Scan not started";
  bacnetRescanSource = "";
  bacnetScanStoredObjects = 0;
  lastLoggedScanPhase = BacnetObjectListScanPhase::Idle;
  lastLoggedScanIndex = 0;
  bacnetDeviceSelected = false;
  bacnetRescanPending = false;
  bacnetScanRequested = false;
  bacnetScanRunning = false;
  bacnetScanFinished = false;
}

static String bacnetDeviceSummary() {
  if (!activeBacnetSession) {
    return "No device selected";
  }

  String summary = "ID ";
  summary += String(activeBacnetSession->deviceInstance());
  summary += " @ ";
  summary += activeBacnetSession->address().toString();
  summary += ":";
  summary += String(activeBacnetSession->port());
  if (activeBacnetVendorId > 0) {
    summary += " vendor ";
    summary += String(activeBacnetVendorId);
  }
  return summary;
}

static String bacnetPropertySummary(size_t propertyIndex) {
  if (!activeBacnetSession || propertyIndex >= kBacnetPreviewPropertyCount) {
    return "not read";
  }
  const BacnetPropertyPreview& property = bacnetDeviceProperties[propertyIndex];
  if (property.status == BacnetDeviceSessionReadStatus::Ack) {
    return property.value;
  }
  return bacnetReadStatusText(property.status);
}

static String bacnetValueObjectRow(const BacnetValueObjectPreview& object) {
  String row = objectTypePrefix(object.object.type);
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
                                        size_t count,
                                        bool scanFinished,
                                        const char* categoryName) {
  if (!activeBacnetSession) {
    return "No device selected";
  }

  String summary;
  for (size_t i = 0; i < count; ++i) {
    if (!objects[i].discovered) {
      continue;
    }
    if (summary.length()) {
      summary += "\n";
    }
    summary += bacnetValueObjectRow(objects[i]);
  }

  if (summary.length()) {
    return summary;
  }
  if (bacnetScanRunning) {
    return bacnetScanStatus.length() ? bacnetScanStatus : "Scanning...";
  }
  if (scanFinished) {
    if (bacnetScanStoredObjects > 0) {
      String emptyCategory = "No ";
      emptyCategory += categoryName;
      emptyCategory += " objects found";
      return emptyCategory;
    }
    return bacnetScanStatus.length() ? bacnetScanStatus : "No objects found";
  }
  return bacnetScanStatus.length() ? bacnetScanStatus : "Waiting for scan";
}

static bool firstDiscoveredObject(const BacnetValueObjectPreview* objects,
                                  size_t count,
                                  BacnetObjectId& object,
                                  String& label) {
  for (size_t i = 0; i < count; ++i) {
    if (!objects[i].discovered) {
      continue;
    }
    object = objects[i].object;
    label = objects[i].label;
    return true;
  }
  return false;
}

static bool selectedStatusObject(BacnetObjectId& object, String& label) {
  if (BACNET_STATUS_OBJECT_INSTANCE != 0) {
    object = BacnetObjectId{static_cast<uint16_t>(BACNET_STATUS_OBJECT_TYPE),
                            BACNET_STATUS_OBJECT_INSTANCE};
    label = objectTypePrefix(object.type) + String(object.instance);
    return true;
  }

  if (firstDiscoveredObject(analogValues, kBacnetMaxFoundObjectsToDisplay,
                            object, label)) {
    return true;
  }
  if (firstDiscoveredObject(multiStateValues, kBacnetMaxFoundObjectsToDisplay,
                            object, label)) {
    return true;
  }
  return firstDiscoveredObject(binaryValues, kBacnetMaxFoundObjectsToDisplay,
                               object, label);
}

static String statusFlagsSummary(const BacnetObjectStatus& status) {
  if (status.statusFlagsStatus != BacnetPropertyReadStatus::Ack) {
    return bacnetPropertyReadStatusText(status.statusFlagsStatus);
  }

  String text;
  text += status.statusFlags.inAlarm ? "alarm" : "no-alarm";
  text += ",";
  text += status.statusFlags.fault ? "fault" : "no-fault";
  text += ",";
  text += status.statusFlags.overridden ? "overridden" : "normal";
  text += ",";
  text += status.statusFlags.outOfService ? "oos" : "in-service";
  return text;
}

static String enumPropertySummary(BacnetPropertyReadStatus status,
                                  const char* text) {
  if (status == BacnetPropertyReadStatus::Ack) {
    return text != nullptr ? text : "";
  }
  return bacnetPropertyReadStatusText(status);
}

static String boolPropertySummary(BacnetPropertyReadStatus status,
                                  bool value) {
  if (status == BacnetPropertyReadStatus::Ack) {
    return value ? "true" : "false";
  }
  return bacnetPropertyReadStatusText(status);
}

static String bacnetObjectDisplayName(const BacnetObjectId& object) {
  String name = objectTypePrefix(object.type);
  name += String(object.instance);
  return name;
}

static String bacnetScanTerminalStatus(const BacnetObjectScanResult& scan) {
  if (scan.objectListCountStatus != BacnetDeviceSessionReadStatus::Ack) {
    String status = "Object-list unavailable: ";
    status += bacnetReadStatusText(scan.objectListCountStatus);
    return status;
  }
  if (scan.found == 0) {
    return "Scan completed with zero accepted process objects";
  }
  if (scan.stored == 0) {
    return "Scan completed with no displayable process objects";
  }

  String status = "Scan complete: ";
  status += String(static_cast<unsigned>(scan.stored));
  status += " process objects";
  if (scan.truncated) {
    status += " (truncated)";
  }
  return status;
}

static const char* bacnetSubscriptionReasonText(
    const BacnetSubscriptionNotification& notification) {
  if (notification.firstValue) {
    return "first";
  }
  if (notification.valueChanged) {
    return "changed";
  }
  if (notification.statusChanged) {
    return "status";
  }
  return "update";
}

static void logScanProgressIfChanged() {
  if (!bacnetScanRunning) {
    return;
  }

  const BacnetObjectListScanProgress progress = scanJob.progress();
  const bool phaseChanged = progress.phase != lastLoggedScanPhase;
  const bool indexedMilestone =
      progress.phase == BacnetObjectListScanPhase::ReadObjectListEntry &&
      progress.currentIndex != lastLoggedScanIndex &&
      (progress.currentIndex == 1 || progress.currentIndex % 25 == 0);

  if (!phaseChanged && !indexedMilestone) {
    return;
  }

  lastLoggedScanPhase = progress.phase;
  lastLoggedScanIndex = progress.currentIndex;
  bacnetGuiLog(
      LogLevel::Info,
      "scan progress phase=%s index=%lu count=%lu found=%u stored=%u",
      bacnetObjectListScanPhaseText(progress.phase),
      static_cast<unsigned long>(progress.currentIndex),
      static_cast<unsigned long>(progress.summary.objectListCount),
      static_cast<unsigned>(progress.summary.found),
      static_cast<unsigned>(progress.summary.stored));
}

static void readBacnetObjectStatusPreview(BacnetDeviceSession& session) {
  BacnetObjectId object;
  String label;
  if (!selectedStatusObject(object, label)) {
    bacnetStatusPreview = BacnetObjectStatusPreview{};
    bacnetStatusPreview.label =
        bacnetScanStatus.length() ? bacnetScanStatus : "No process object found";
    return;
  }

  BacnetObjectStatus status;
  session.readObjectStatus(object, status, kBacnetScanReadTimeoutMs, true);

  bacnetStatusPreview.available = true;
  bacnetStatusPreview.object = object;
  bacnetStatusPreview.label =
      label.length() ? label : objectTypePrefix(object.type) + String(object.instance);
  bacnetStatusPreview.presentValue =
      propertyValueTextIfAck(status.presentValueStatus, status.presentValue);
  bacnetStatusPreview.presentValueStatus =
      bacnetPropertyReadStatusText(status.presentValueStatus);
  bacnetStatusPreview.state = bacnetObjectHealthStateText(status.state);
  bacnetStatusPreview.flags = statusFlagsSummary(status);
  bacnetStatusPreview.eventState =
      enumPropertySummary(status.eventStateStatus,
                          bacnetEventStateText(status.eventState));
  bacnetStatusPreview.reliability =
      enumPropertySummary(status.reliabilityStatus,
                          bacnetReliabilityText(status.reliability));
  bacnetStatusPreview.outOfService =
      boolPropertySummary(status.outOfServiceStatus, status.outOfService);
}

static String bacnetObjectStatusSummary() {
  if (!activeBacnetSession) {
    return "No device selected";
  }
  if (!bacnetStatusPreview.available) {
    return bacnetStatusPreview.label;
  }

  String summary = objectTypePrefix(bacnetStatusPreview.object.type);
  summary += String(bacnetStatusPreview.object.instance);
  summary += ": ";
  summary += shortenBacnetLabel(bacnetStatusPreview.label);
  summary += "\nstate=";
  summary += bacnetStatusPreview.state;
  summary += " pv=";
  summary += bacnetStatusPreview.presentValue;
  summary += " pv-status=";
  summary += bacnetStatusPreview.presentValueStatus;
  summary += "\nflags=";
  summary += bacnetStatusPreview.flags;
  summary += " event-state=";
  summary += bacnetStatusPreview.eventState;
  summary += "\nreliability=";
  summary += bacnetStatusPreview.reliability;
  summary += " oos=";
  summary += bacnetStatusPreview.outOfService;
  return summary;
}

static String bacnetAnalogValuesSummary() {
  return bacnetValueObjectsSummary(analogValues, kBacnetMaxFoundObjectsToDisplay,
                                   bacnetScanFinished, "analog");
}

static String bacnetBinaryValuesSummary() {
  return bacnetValueObjectsSummary(binaryValues, kBacnetMaxFoundObjectsToDisplay,
                                   bacnetScanFinished, "binary");
}

static String bacnetMultiStateValuesSummary() {
  return bacnetValueObjectsSummary(multiStateValues,
                                   kBacnetMaxFoundObjectsToDisplay,
                                   bacnetScanFinished, "multi-state");
}

static void clearBacnetProcessObjectPreview(const char* status) {
  resetValueObjects(analogValues, kBacnetMaxFoundObjectsToDisplay);
  resetValueObjects(binaryValues, kBacnetMaxFoundObjectsToDisplay);
  resetValueObjects(multiStateValues, kBacnetMaxFoundObjectsToDisplay);
  bacnetStatusPreview = BacnetObjectStatusPreview{};
  bacnetStatusPreview.label = status;
  bacnetScanStoredObjects = 0;
}

static void requestBacnetRescan(const char* source) {
  const char* safeSource = source != nullptr ? source : "unknown";
  bacnetGuiLog(LogLevel::Info, "rescan requested source=%s", safeSource);

  if (!activeBacnetSession || !bacnetDeviceSelected) {
    bacnetGuiLog(LogLevel::Warn,
                 "rescan rejected source=%s reason=no device selected",
                 safeSource);
    return;
  }
  if (bacnetScanRunning || scanJob.isActive()) {
    bacnetGuiLog(LogLevel::Warn,
                 "rescan rejected source=%s reason=scan already running",
                 safeSource);
    return;
  }
  if (bacnetRescanPending || bacnetScanRequested) {
    bacnetGuiLog(LogLevel::Info,
                 "rescan accepted source=%s state=already queued",
                 safeSource);
    return;
  }

  bacnetRescanPending = true;
  bacnetRescanSource = safeSource;
  bacnetScanStatus = "Rescan queued";
  bacnetStatusPreview.available = false;
  bacnetStatusPreview.label = "Rescan queued";
  bacnetGuiLog(LogLevel::Info, "scan queued source=%s", safeSource);
}

static void processPendingBacnetRescan() {
  if (!bacnetRescanPending) {
    return;
  }

  const String source = bacnetRescanSource.length() ? bacnetRescanSource
                                                   : String("unknown");
  bacnetRescanPending = false;
  bacnetRescanSource = "";

  if (!activeBacnetSession || !bacnetDeviceSelected) {
    bacnetScanStatus = "Rescan rejected: no device selected";
    bacnetStatusPreview.available = false;
    bacnetStatusPreview.label = bacnetScanStatus;
    bacnetGuiLog(LogLevel::Warn,
                 "rescan rejected source=%s reason=no device selected",
                 source.c_str());
    return;
  }
  if (bacnetScanRunning || scanJob.isActive()) {
    bacnetGuiLog(LogLevel::Warn,
                 "rescan rejected source=%s reason=scan already running",
                 source.c_str());
    return;
  }

  clearBacnetProcessObjectPreview("Rescanning");
  bacnetScanStatus = "Rescanning";
  bacnetScanRequested = true;
  bacnetScanFinished = false;
  bacnetGuiLog(LogLevel::Info, "rescan accepted source=%s scan queued",
               source.c_str());
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

  auto bacnetDeviceGroup = ConfigManager.liveGroup("bacnet")
                                .page("Sensors", 10)
                                .card("BACnet/IP Client", 20)
                                .group("Selected Device", 1);

  bacnetDeviceGroup.value("device0", []() { return bacnetDeviceSummary(); })
      .label("Device")
      .order(10);

  bacnetDeviceGroup.button("device0_rescan", "Scan / Rescan",
                           []() { requestBacnetRescan("ui"); })
      .order(15);

  for (size_t propertyIndex = 0; propertyIndex < kBacnetPreviewPropertyCount;
       ++propertyIndex) {
    const String propertyKey =
        String("device0_") + kBacnetPreviewProperties[propertyIndex].name;
    bacnetDeviceGroup
        .value(propertyKey.c_str(),
               [propertyIndex]() { return bacnetPropertySummary(propertyIndex); })
        .label(kBacnetPreviewProperties[propertyIndex].name)
        .order(20 + propertyIndex);
  }

  bacnetDeviceGroup.divider("Analog Objects", 39);
  bacnetDeviceGroup.value("device0_analogValues",
                          []() { return bacnetAnalogValuesSummary(); })
      .label("Analog")
      .addCSSClass("bacnetObjectListValue")
      .order(40);

  bacnetDeviceGroup.divider("Binary Objects", 49);
  bacnetDeviceGroup.value("device0_binaryValues",
                          []() { return bacnetBinaryValuesSummary(); })
      .label("Binary")
      .addCSSClass("bacnetObjectListValue")
      .order(50);

  bacnetDeviceGroup.divider("Multi-State Objects", 59);
  bacnetDeviceGroup.value("device0_multiStateValues",
                          []() { return bacnetMultiStateValuesSummary(); })
      .label("Multi-State")
      .addCSSClass("bacnetObjectListValue")
      .order(60);

  bacnetDeviceGroup.divider("Object Health", 69);
  bacnetDeviceGroup.value("device0_objectHealth",
                          []() { return bacnetObjectStatusSummary(); })
      .label("State")
      .addCSSClass("bacnetObjectListValue")
      .order(70);
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
  if (!bacnetStarted || !bacnetAddressConfigValid) {
    return;
  }

  if (!bacnetClient.sendWhoIs(whoIsDestination)) {
    Serial.println("[W] BACnet Who-Is send failed");
    bacnetGuiLog(LogLevel::Warn, "Who-Is send failed");
    return;
  }

  lastWhoIsAt = millis();
}

static void readDeviceProperties(BacnetDeviceSession& session) {
  for (size_t i = 0; i < kBacnetPreviewPropertyCount; ++i) {
    BacnetValue value;
    BacnetPropertyPreview& property = bacnetDeviceProperties[i];
    property.status =
        session.object(session.deviceObject())
            .readProperty(property.id, value, kBacnetScanReadTimeoutMs);
    property.value = valueTextIfAck(property.status, value);
  }
}

static void onPresentValueUpdate(
    const BacnetSubscriptionNotification& notification) {
  auto* preview =
      static_cast<BacnetValueObjectPreview*>(notification.userData);
  if (preview == nullptr) {
    return;
  }

  preview->presentValueStatus = bacnetReadStatusText(notification.status);
  if (notification.status == BacnetDeviceSessionReadStatus::Ack &&
      notification.hasValue && notification.value != nullptr) {
    preview->presentValue = notification.value->displayText();
  }
  bacnetGuiLog(LogLevel::Info, "subscription update %s present-value %s=%s",
               bacnetObjectDisplayName(notification.objectId).c_str(),
               bacnetSubscriptionReasonText(notification),
               preview->presentValue.length() ? preview->presentValue.c_str()
                                              : preview->presentValueStatus.c_str());
}

static bool subscribePresentValue(BacnetDeviceSession& session,
                                  BacnetValueObjectPreview& preview) {
  if (!preview.discovered) {
    return false;
  }

  BacnetSubscribeOptions options;
  options.fallbackPollMs = kBacnetSubscriptionFallbackPollMs;
  options.timeoutMs = kBacnetScanReadTimeoutMs;
  options.immediateFirstRead = true;
  options.notifyOnStatusChange = true;

  preview.subscription.reset(new BacnetPropertySubscription(
      session.object(preview.object)
          .property(BacnetPropertyId::PresentValue)
          .subscribe(onPresentValueUpdate, &preview, options)));
  bacnetGuiLog(LogLevel::Info,
               "subscription created %s present-value fallbackMs=%lu active=%s",
               bacnetObjectDisplayName(preview.object).c_str(),
               static_cast<unsigned long>(options.fallbackPollMs),
               preview.subscription && preview.subscription->active() ? "yes"
                                                                       : "no");
  return preview.subscription && preview.subscription->active();
}

static bool copyScannedObjectToPreview(const BacnetScannedObject& scanned,
                                       BacnetValueObjectPreview* destination,
                                       size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (destination[i].discovered) {
      continue;
    }

    destination[i].discovered = true;
    destination[i].object = scanned.objectId;
    destination[i].objectName =
        valueTextIfAck(scanned.objectNameStatus, scanned.objectName);
    destination[i].description =
        valueTextIfAck(scanned.descriptionStatus, scanned.description);
    destination[i].label = objectLabel(scanned);
    destination[i].presentValue =
        scanned.presentValueStatus == BacnetDeviceSessionReadStatus::Ack
            ? scanned.presentValue.displayText()
            : "";
    destination[i].presentValueStatus =
        bacnetReadStatusText(scanned.presentValueStatus);
    return true;
  }
  return false;
}

static void copyScanResultsToPreviews(const BacnetObjectScanResult& scan,
                                      size_t& analogStored,
                                      size_t& binaryStored,
                                      size_t& multiStateStored) {
  analogStored = 0;
  binaryStored = 0;
  multiStateStored = 0;

  for (size_t i = 0; i < scan.stored; ++i) {
    if (isAnalogProcessObject(scanBuffer[i].objectId.type)) {
      if (copyScannedObjectToPreview(scanBuffer[i], analogValues,
                                     kBacnetMaxFoundObjectsToDisplay)) {
        ++analogStored;
      }
    } else if (isBinaryProcessObject(scanBuffer[i].objectId.type)) {
      if (copyScannedObjectToPreview(scanBuffer[i], binaryValues,
                                     kBacnetMaxFoundObjectsToDisplay)) {
        ++binaryStored;
      }
    } else if (isMultiStateProcessObject(scanBuffer[i].objectId.type)) {
      if (copyScannedObjectToPreview(scanBuffer[i], multiStateValues,
                                     kBacnetMaxFoundObjectsToDisplay)) {
        ++multiStateStored;
      }
    }
  }
}

static bool readConfiguredFallbackObject(BacnetDeviceSession& session,
                                         BacnetObjectId object,
                                         BacnetValueObjectPreview* destination,
                                         size_t destinationCount) {
  if (object.instance == 0) {
    return false;
  }

  BacnetScannedObject scanned;
  scanned.objectId = object;
  scanned.objectNameStatus =
      session.object(object).readProperty(BacnetPropertyId::ObjectName,
                                          scanned.objectName,
                                          kBacnetScanReadTimeoutMs);
  scanned.descriptionStatus =
      session.object(object).readProperty(BacnetPropertyId::Description,
                                          scanned.description,
                                          kBacnetScanReadTimeoutMs);
  scanned.presentValueStatus =
      session.object(object).readProperty(BacnetPropertyId::PresentValue,
                                          scanned.presentValue,
                                          kBacnetScanReadTimeoutMs);

  bacnetGuiLog(LogLevel::Info,
               "fallback probe %s objectName=%s description=%s present-value=%s",
               bacnetObjectDisplayName(object).c_str(),
               bacnetReadStatusText(scanned.objectNameStatus),
               bacnetReadStatusText(scanned.descriptionStatus),
               bacnetReadStatusText(scanned.presentValueStatus));

  if (scanned.objectNameStatus != BacnetDeviceSessionReadStatus::Ack &&
      scanned.descriptionStatus != BacnetDeviceSessionReadStatus::Ack &&
      scanned.presentValueStatus != BacnetDeviceSessionReadStatus::Ack) {
    bacnetGuiLog(LogLevel::Warn, "fallback object %s unavailable",
                 bacnetObjectDisplayName(object).c_str());
    return false;
  }

  if (!copyScannedObjectToPreview(scanned, destination, destinationCount)) {
    bacnetGuiLog(LogLevel::Warn, "fallback object %s not stored: display full",
                 bacnetObjectDisplayName(object).c_str());
    return false;
  }
  return true;
}

static size_t appendConfiguredFallbackObjects(BacnetDeviceSession& session,
                                              size_t& analogStored,
                                              size_t& binaryStored,
                                              size_t& multiStateStored) {
  size_t loaded = 0;
  bacnetGuiLog(LogLevel::Info, "configured fallback probe started");

  if (readConfiguredFallbackObject(
          session,
          BacnetObjectId{BACNET_FALLBACK_ANALOG_OBJECT_TYPE,
                         BACNET_FALLBACK_ANALOG_OBJECT_INSTANCE},
          analogValues, kBacnetMaxFoundObjectsToDisplay)) {
    ++analogStored;
    ++loaded;
  }
  if (readConfiguredFallbackObject(
          session,
          BacnetObjectId{BACNET_FALLBACK_BINARY_OBJECT_TYPE,
                         BACNET_FALLBACK_BINARY_OBJECT_INSTANCE},
          binaryValues, kBacnetMaxFoundObjectsToDisplay)) {
    ++binaryStored;
    ++loaded;
  }
  if (readConfiguredFallbackObject(
          session,
          BacnetObjectId{BACNET_FALLBACK_MULTISTATE_OBJECT_TYPE,
                         BACNET_FALLBACK_MULTISTATE_OBJECT_INSTANCE},
          multiStateValues, kBacnetMaxFoundObjectsToDisplay)) {
    ++multiStateStored;
    ++loaded;
  }

  bacnetGuiLog(LogLevel::Info, "configured fallback probe complete loaded=%u",
               static_cast<unsigned>(loaded));
  return loaded;
}

static BacnetObjectScanOptions makeValueObjectScanOptions() {
  BacnetObjectScanOptions options;
  bacnetSetObjectTypeFilter(options, kValueObjectScanFilter);
  options.maxObjectListEntries = kBacnetMaxObjectListEntriesToInspect;
  options.readTimeoutMs = kBacnetScanReadTimeoutMs;
  options.readObjectName = true;
  options.readDescription = true;
  options.readPresentValue = true;
  return options;
}

static bool beginValueObjectScan(BacnetDeviceSession& session) {
  for (size_t i = 0; i < kBacnetScanResultCapacity; ++i) {
    scanBuffer[i] = BacnetScannedObject{};
  }

  const BacnetObjectScanOptions options = makeValueObjectScanOptions();
  bacnetScanStatus = "Object-list scan starting";
  lastLoggedScanPhase = BacnetObjectListScanPhase::Idle;
  lastLoggedScanIndex = 0;
  bacnetGuiLog(LogLevel::Info,
               "scan start requested maxEntries=%lu capacity=%u filter=%u",
               static_cast<unsigned long>(options.maxObjectListEntries),
               static_cast<unsigned>(kBacnetScanResultCapacity),
               static_cast<unsigned>(sizeof(kValueObjectScanFilter) /
                                     sizeof(kValueObjectScanFilter[0])));
  if (!session.beginObjectListScan(scanJob, options, scanBuffer,
                                   kBacnetScanResultCapacity)) {
    bacnetScanStatus = "Scan start failed: ";
    bacnetScanStatus += bacnetObjectListScanJobStatusText(scanJob.status());
    bacnetGuiLog(LogLevel::Warn,
                 "scan start failed status=%s phase=%s count-status=%s",
                 bacnetObjectListScanJobStatusText(scanJob.status()),
                 bacnetObjectListScanPhaseText(scanJob.phase()),
                 bacnetReadStatusText(scanJob.summary().objectListCountStatus));
    return false;
  }
  bacnetScanStatus = "Object-list scan running";
  bacnetGuiLog(LogLevel::Info, "object-list count read started");
  return true;
}

static void finishValueObjectScan(BacnetDeviceSession& session,
                                  const BacnetObjectScanResult& scan) {
  size_t analogStored = 0;
  size_t binaryStored = 0;
  size_t multiStateStored = 0;
  copyScanResultsToPreviews(scan, analogStored, binaryStored, multiStateStored);

  const bool needsConfiguredFallback =
      scan.objectListCountStatus != BacnetDeviceSessionReadStatus::Ack ||
      scan.stored == 0;
  size_t fallbackStored = 0;
  if (needsConfiguredFallback) {
    fallbackStored = appendConfiguredFallbackObjects(
        session, analogStored, binaryStored, multiStateStored);
  }

  bacnetScanStoredObjects = analogStored + binaryStored + multiStateStored;
  if (fallbackStored > 0) {
    bacnetScanStatus = "Configured fallback objects loaded: ";
    bacnetScanStatus += String(static_cast<unsigned>(fallbackStored));
  } else {
    bacnetScanStatus = bacnetScanTerminalStatus(scan);
  }

  size_t subscriptionsCreated = 0;
  for (size_t i = 0; i < kBacnetMaxFoundObjectsToDisplay; ++i) {
    if (subscribePresentValue(session, analogValues[i])) {
      ++subscriptionsCreated;
    }
    if (subscribePresentValue(session, binaryValues[i])) {
      ++subscriptionsCreated;
    }
    if (subscribePresentValue(session, multiStateValues[i])) {
      ++subscriptionsCreated;
    }
  }

  readBacnetObjectStatusPreview(session);

  bacnetScanFinished = true;
  bacnetScanRunning = false;
  bacnetGuiLog(LogLevel::Info,
               "scan terminal status=%s count-status=%s count=%lu inspected=%lu found=%u stored=%u analog=%u binary=%u multistate=%u truncated=%s",
               bacnetObjectListScanJobStatusText(scanJob.status()),
               bacnetReadStatusText(scan.objectListCountStatus),
               static_cast<unsigned long>(scan.objectListCount),
               static_cast<unsigned long>(scan.inspected),
               static_cast<unsigned>(scan.found),
               static_cast<unsigned>(scan.stored),
               static_cast<unsigned>(analogStored),
               static_cast<unsigned>(binaryStored),
               static_cast<unsigned>(multiStateStored),
               scan.truncated ? "yes" : "no");
  if (fallbackStored > 0) {
    bacnetGuiLog(LogLevel::Info,
                 "using configured fallback objects loaded=%u total=%u",
                 static_cast<unsigned>(fallbackStored),
                 static_cast<unsigned>(bacnetScanStoredObjects));
  } else if (scan.stored == 0 ||
             scan.objectListCountStatus != BacnetDeviceSessionReadStatus::Ack) {
    bacnetGuiLog(LogLevel::Warn, "scan result: %s",
                 bacnetScanStatus.c_str());
  }
  bacnetGuiLog(LogLevel::Info, "subscriptions recreated count=%u",
               static_cast<unsigned>(subscriptionsCreated));
}

static void scanSelectedBacnetDevice() {
  if (!activeBacnetSession || bacnetScanFinished) {
    return;
  }

  if (bacnetScanRequested && !bacnetScanRunning) {
    bacnetScanRequested = false;
    bacnetScanRunning = true;
    bacnetScanStatus = "Reading Device properties";
    bacnetGuiLog(
        LogLevel::Info, "scanning selected device %lu",
        static_cast<unsigned long>(activeBacnetSession->deviceInstance()));
    bacnetGuiLog(LogLevel::Info, "device metadata read started");

    readDeviceProperties(*activeBacnetSession);
    bacnetGuiLog(LogLevel::Info, "device metadata read complete");
    if (!beginValueObjectScan(*activeBacnetSession)) {
      bacnetScanRunning = false;
      bacnetScanFinished = true;
      readBacnetObjectStatusPreview(*activeBacnetSession);
      return;
    }
  }

  if (!bacnetScanRunning) {
    return;
  }

  activeBacnetSession->pollObjectListScan(scanJob, millis());
  logScanProgressIfChanged();
  if (scanJob.isTerminal()) {
    finishValueObjectScan(*activeBacnetSession, scanJob.summary());
  }
}

static void selectBacnetDevice(uint32_t deviceInstance,
                               IPAddress address,
                               uint16_t port,
                               uint16_t vendorId) {
  if (bacnetDeviceSelected || isZeroBacnetAddress(address)) {
    return;
  }

  resetBacnetPreviews();
  activeBacnetVendorId = vendorId;
  activeBacnetSession.reset(new BacnetDeviceSession(
      BacnetDeviceSession::fromEndpoint(bacnetClient, deviceInstance, address,
                                        port)));
  bacnetDeviceSelected = true;
  bacnetScanStatus = "Scan queued for selected device";

  Serial.print("[I] BACnet selected device ");
  Serial.print(deviceInstance);
  Serial.print(" at ");
  Serial.print(address);
  Serial.print(":");
  Serial.println(port);
  bacnetGuiLog(LogLevel::Info, "selected device %lu at %s:%u",
               static_cast<unsigned long>(deviceInstance),
               address.toString().c_str(), static_cast<unsigned>(port));

  bacnetScanRequested = true;
  bacnetGuiLog(LogLevel::Info, "scan queued for selected device %lu",
               static_cast<unsigned long>(deviceInstance));
}

static void selectBacnetDevice(const BacnetIAmDevice& device) {
  if (bacnetDeviceSelected || isZeroBacnetAddress(device.address)) {
    return;
  }

  resetBacnetPreviews();
  activeBacnetVendorId = device.vendorId;
  activeBacnetSession.reset(new BacnetDeviceSession(
      BacnetDeviceSession::fromIAm(bacnetClient, device)));
  bacnetDeviceSelected = true;
  bacnetScanStatus = "Scan queued for selected device";

  Serial.print("[I] BACnet selected device ");
  Serial.print(device.deviceInstance);
  Serial.print(" at ");
  Serial.print(device.address);
  Serial.print(":");
  Serial.println(BacnetClient::kDefaultPort);
  bacnetGuiLog(LogLevel::Info, "selected device %lu at %s:%u",
               static_cast<unsigned long>(device.deviceInstance),
               device.address.toString().c_str(),
               static_cast<unsigned>(BacnetClient::kDefaultPort));

  bacnetScanRequested = true;
  bacnetGuiLog(LogLevel::Info, "scan queued for selected device %lu",
               static_cast<unsigned long>(device.deviceInstance));
}

static void clearBacnetRuntime() {
  if (activeBacnetSession && scanJob.isActive()) {
    activeBacnetSession->cancelObjectListScan(scanJob);
  }
  resetValueObjects(analogValues, kBacnetMaxFoundObjectsToDisplay);
  resetValueObjects(binaryValues, kBacnetMaxFoundObjectsToDisplay);
  resetValueObjects(multiStateValues, kBacnetMaxFoundObjectsToDisplay);
  bacnetStatusPreview = BacnetObjectStatusPreview{};
  activeBacnetSession.reset();
  bacnetDeviceSelected = false;
  bacnetScanRequested = false;
  bacnetScanRunning = false;
  bacnetScanFinished = false;
  bacnetRescanPending = false;
  bacnetScanStatus = "Scan not started";
  bacnetRescanSource = "";
  bacnetScanStoredObjects = 0;
  lastLoggedScanPhase = BacnetObjectListScanPhase::Idle;
  lastLoggedScanIndex = 0;
}

static void startBacnetClient() {
  if (bacnetStarted) {
    return;
  }

  if (!bacnetAddressConfigValid) {
    if (!bacnetAddressConfigErrorLogged) {
      bacnetAddressConfigErrorLogged = true;
      Serial.println("[E] BACnet disabled: invalid BACnet IP config");
      bacnetGuiLog(LogLevel::Error, "disabled: invalid BACnet IP config");
    }
    return;
  }

  if (!bacnetClient.begin()) {
    Serial.println("[E] BACnet UDP listener failed to start");
    bacnetGuiLog(LogLevel::Error, "client UDP listener failed");
    return;
  }

  bacnetStarted = true;
  Serial.print("[I] BACnet client started on UDP port ");
  Serial.println(bacnetClient.localPort());

  sendWhoIs();
  selectBacnetDevice(BACNET_TARGET_DEVICE_INSTANCE,
                     configuredBacnetTargetAddress, BACNET_TARGET_PORT, 0);
}

static void pollBacnetDiscovery() {
  if (!bacnetStarted) {
    return;
  }

  BacnetIAmDevice device;
  if (bacnetClient.pollIAm(device) && !bacnetDeviceSelected) {
    selectBacnetDevice(device);
  }

  if (!bacnetDeviceSelected && millis() - lastWhoIsAt >= kWhoIsIntervalMs) {
    sendWhoIs();
  }
}

static void pollBacnetSubscriptions() {
  if (!activeBacnetSession || bacnetScanRunning) {
    return;
  }

  const uint32_t now = millis();
  for (size_t i = 0; i < kBacnetMaxFoundObjectsToDisplay; ++i) {
    if (analogValues[i].subscription) {
      activeBacnetSession->poll(*analogValues[i].subscription, now);
    }
    if (binaryValues[i].subscription) {
      activeBacnetSession->poll(*binaryValues[i].subscription, now);
    }
    if (multiStateValues[i].subscription) {
      activeBacnetSession->poll(*multiStateValues[i].subscription, now);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("[I] BACnet client demo starting");

  ConfigManagerClass::setLogger([](const char* msg) {
    Serial.print("[D] CM ");
    Serial.println(msg);
  });

  resetBacnetPreviews();
  ConfigManager.setAppName(APP_NAME);
  ConfigManager.setAppTitle(APP_NAME);
  ConfigManager.setVersion(APP_VERSION);
  ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);
  ConfigManager.enableBuiltinSystemProvider();
  ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE,
                             sizeof(GLOBAL_THEME_OVERRIDE) - 1);
  setupGuiLogging();
  configureBacnetAddresses();

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
  startBacnetClient();
}

void onWiFiDisconnected() {
  wifiServices.onDisconnected();
  clearBacnetRuntime();
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
  bacnetClient.logger().tick();
  alarmManager.update();
  pollBacnetDiscovery();
  processPendingBacnetRescan();
  pollBacnetSubscriptions();
  scanSelectedBacnetDevice();

  static unsigned long lastLoopLog = 0;
  if (millis() - lastLoopLog > 60000) {
    lastLoopLog = millis();
    Serial.print("[D] Loop running, WiFi status=");
    Serial.print(WiFi.status());
    Serial.print(" heap=");
    Serial.println(ESP.getFreeHeap());
  }
}
