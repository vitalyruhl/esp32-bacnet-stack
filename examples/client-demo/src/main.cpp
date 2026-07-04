// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <Arduino.h>

#include <EspBacnet.h>
#include <WiFi.h>

#include "ConfigManager.h"
#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"

#include "BacnetDemoFallbackObjects.h"
#include "BacnetDemoFormat.h"
#include "BacnetDemoLogging.h"
#include "BacnetDemoWatchedAnalogValue.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#define CM_HAS_WIFI_SECRETS 1
#else
#define CM_HAS_WIFI_SECRETS 0
#endif

#ifndef APP_VERSION
#define APP_VERSION "0.24.1"
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

extern ConfigManagerClass ConfigManager;

static cm::CoreSettings& coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings& systemSettings = coreSettings.system;
static cm::CoreWiFiSettings& wifiSettings = coreSettings.wifi;
static cm::CoreNtpSettings& ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;

static BacnetClient bacnetClient;
static BacnetDemoLogging demoLogging(ConfigManager, bacnetClient);

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

#ifndef BACNET_FALLBACK_ANALOG_OBJECT_INSTANCE
#define BACNET_FALLBACK_ANALOG_OBJECT_INSTANCE 220
#endif
#ifndef BACNET_FALLBACK_BINARY_OBJECT_INSTANCE
#define BACNET_FALLBACK_BINARY_OBJECT_INSTANCE 320
#endif
#ifndef BACNET_FALLBACK_MULTISTATE_OBJECT_INSTANCE
#define BACNET_FALLBACK_MULTISTATE_OBJECT_INSTANCE 2020
#endif

#ifndef BACNET_WATCHED_ANALOG_VALUE_INSTANCE
#define BACNET_WATCHED_ANALOG_VALUE_INSTANCE 200
#endif

static constexpr uint32_t kWhoIsIntervalMs = 30000;
static constexpr uint32_t kBacnetScanReadTimeoutMs = 3000;
static constexpr uint32_t kBacnetMaxObjectListEntriesToInspect = 600;
static constexpr size_t kBacnetMaxFoundObjectsToDisplay = 3;
static constexpr size_t kBacnetScanResultCapacity = kBacnetMaxFoundObjectsToDisplay * 3;
static constexpr size_t kBacnetPreviewPropertyCount = 4;
static constexpr uint32_t kBacnetSubscriptionFallbackPollMs = 30000;
static constexpr uint32_t kWatchedAnalogValuePollMs = 3000;

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
static BacnetObjectListScanPhase lastLoggedScanPhase = BacnetObjectListScanPhase::Idle;
static uint32_t lastLoggedScanIndex = 0;
static unsigned long lastWhoIsAt = 0;

struct BacnetPropertySpec {
  const char* name = "";
  BacnetPropertyId id = BacnetPropertyId::ObjectName;
};

struct BacnetPropertyPreview {
  const char* name = "";
  BacnetPropertyId id = BacnetPropertyId::ObjectName;
  BacnetValue value;
  BacnetDeviceSessionReadStatus status = BacnetDeviceSessionReadStatus::Skipped;
};

struct BacnetValueObjectPreview {
  bool discovered = false;
  BacnetObjectId object;
  const BacnetScannedObject* scanned = nullptr;
  BacnetDeviceSessionReadStatus presentValueStatus =
    BacnetDeviceSessionReadStatus::Skipped;
  std::unique_ptr<BacnetPropertySubscription> subscription;
};

struct BacnetObjectStatusPreview {
  bool available = false;
  BacnetObjectId object;
  const BacnetScannedObject* labelSource = nullptr;
  BacnetObjectStatus status;
  const char* fallbackStatus = "not read";
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
static BacnetDemoWatchedAnalogValue watchedAnalogValue(
  BACNET_WATCHED_ANALOG_VALUE_INSTANCE,
  kWatchedAnalogValuePollMs,
  kBacnetScanReadTimeoutMs);
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
  demoLogging.log(BacnetDemoLogging::Level::Error, "invalid BACnet %s IP: %s", label, addressText != nullptr ? addressText : "<null>");
  return false;
}

static bool configureBacnetAddresses() {
  bacnetAddressConfigValid =
    parseBacnetAddress("Who-Is destination", BACNET_WHOIS_DESTINATION, whoIsDestination) &&
    parseBacnetAddress("target address", BACNET_TARGET_ADDRESS, configuredBacnetTargetAddress);

  if (bacnetAddressConfigValid) {
    bacnetAddressConfigErrorLogged = false;
  }
  return bacnetAddressConfigValid;
}

static bool isZeroBacnetAddress(const IPAddress& address) {
  return address[0] == 0 && address[1] == 0 && address[2] == 0 &&
         address[3] == 0;
}

static const char* valueTextOrNull(BacnetDeviceSessionReadStatus status,
                                   const BacnetValue& value) {
  return status == BacnetDeviceSessionReadStatus::Ack && value.textLength > 0
           ? value.displayText()
           : nullptr;
}

static const char* scannedObjectNameOrNull(const BacnetScannedObject& scanned) {
  return valueTextOrNull(scanned.objectNameStatus, scanned.objectName);
}

static const char* scannedDescriptionOrNull(const BacnetScannedObject& scanned) {
  return valueTextOrNull(scanned.descriptionStatus, scanned.description);
}

static const char* scannedLabelOrNull(const BacnetScannedObject& scanned) {
  const char* objectName = scannedObjectNameOrNull(scanned);
  if (objectName != nullptr && objectName[0] != '\0') {
    return objectName;
  }
  const char* description = scannedDescriptionOrNull(scanned);
  if (description != nullptr && description[0] != '\0') {
    return description;
  }
  return nullptr;
}

static void appendObjectDisplayName(FixedTextBuffer& out,
                                    const BacnetObjectId& object) {
  out.append(bacnetObjectTypePrefix(object.type));
  out.appendFormat("%lu", static_cast<unsigned long>(object.instance));
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
    bacnetDeviceProperties[i].value = BacnetValue{};
    bacnetDeviceProperties[i].status = BacnetDeviceSessionReadStatus::Skipped;
  }
  resetValueObjects(analogValues, kBacnetMaxFoundObjectsToDisplay);
  resetValueObjects(binaryValues, kBacnetMaxFoundObjectsToDisplay);
  resetValueObjects(multiStateValues, kBacnetMaxFoundObjectsToDisplay);
  bacnetStatusPreview = BacnetObjectStatusPreview{};
  watchedAnalogValue.reset("not read");
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

static void formatBacnetDeviceSummary(FixedTextBuffer& out) {
  if (!activeBacnetSession) {
    out.append("No device selected");
    return;
  }

  const IPAddress address = activeBacnetSession->address();
  out.appendFormat("ID %lu @ %u.%u.%u.%u:%u",
                   static_cast<unsigned long>(
                     activeBacnetSession->deviceInstance()),
                   static_cast<unsigned>(address[0]),
                   static_cast<unsigned>(address[1]),
                   static_cast<unsigned>(address[2]),
                   static_cast<unsigned>(address[3]),
                   static_cast<unsigned>(activeBacnetSession->port()));
  if (activeBacnetVendorId > 0) {
    out.appendFormat(" vendor %u", static_cast<unsigned>(activeBacnetVendorId));
  }
}

static void formatBacnetPropertySummary(size_t propertyIndex,
                                        FixedTextBuffer& out) {
  if (!activeBacnetSession || propertyIndex >= kBacnetPreviewPropertyCount) {
    out.append("not read");
    return;
  }
  const BacnetPropertyPreview& property = bacnetDeviceProperties[propertyIndex];
  if (property.status == BacnetDeviceSessionReadStatus::Ack) {
    out.append(property.value.displayText());
    return;
  }
  out.append(bacnetReadStatusText(property.status));
}

static const char* valueObjectPresentValueText(
  const BacnetValueObjectPreview& object) {
  if (object.subscription && object.subscription->hasValue()) {
    return object.subscription->lastValue().displayText();
  }
  if (object.scanned != nullptr &&
      object.scanned->presentValueStatus == BacnetDeviceSessionReadStatus::Ack) {
    return object.scanned->presentValue.displayText();
  }
  return nullptr;
}

static BacnetDeviceSessionReadStatus valueObjectPresentValueStatus(
  const BacnetValueObjectPreview& object) {
  return object.subscription ? object.subscription->lastStatus()
                             : object.presentValueStatus;
}

static void formatBacnetValueObjectRow(
  const BacnetValueObjectPreview& object,
  FixedTextBuffer& out) {
  appendObjectDisplayName(out, object.object);
  out.append(": ");
  const char* label =
    object.scanned != nullptr ? scannedLabelOrNull(*object.scanned) : nullptr;
  out.appendShortened(label != nullptr && label[0] != '\0' ? label
                                                           : "<unnamed>");
  const char* description = object.scanned != nullptr
                              ? scannedDescriptionOrNull(*object.scanned)
                              : nullptr;
  if (description != nullptr && description[0] != '\0' &&
      (label == nullptr || std::strcmp(description, label) != 0)) {
    out.append(" | D:");
    out.appendShortened(description);
  }
  out.append(" - V:");
  const char* presentValue = valueObjectPresentValueText(object);
  out.append(presentValue != nullptr && presentValue[0] != '\0'
               ? presentValue
               : bacnetReadStatusText(valueObjectPresentValueStatus(object)));
}

static void formatBacnetValueObjectsSummary(
  const BacnetValueObjectPreview* objects,
  size_t count,
  bool scanFinished,
  const char* categoryName,
  FixedTextBuffer& out) {
  if (!activeBacnetSession) {
    out.append("No device selected");
    return;
  }

  for (size_t i = 0; i < count; ++i) {
    if (!objects[i].discovered) {
      continue;
    }
    if (!out.empty()) {
      out.append('\n');
    }
    formatBacnetValueObjectRow(objects[i], out);
  }

  if (!out.empty()) {
    return;
  }
  if (bacnetScanRunning) {
    out.append(bacnetScanStatus.length() ? bacnetScanStatus.c_str()
                                         : "Scanning...");
    return;
  }
  if (scanFinished) {
    if (bacnetScanStoredObjects > 0) {
      out.append("No ");
      out.append(categoryName);
      out.append(" objects found");
      return;
    }
    out.append(bacnetScanStatus.length() ? bacnetScanStatus.c_str()
                                         : "No objects found");
    return;
  }
  out.append(bacnetScanStatus.length() ? bacnetScanStatus.c_str()
                                       : "Waiting for scan");
}

static void logWatchedAnalogDetails() {
  if (!activeBacnetSession) {
    demoLogging.log(BacnetDemoLogging::Level::Info,
                    "watched AV details unavailable: no device selected");
    return;
  }

  String readStatus = watchedAnalogValue.readStatusSummary();
  readStatus.replace("\n", "; ");
  String status = watchedAnalogValue.statusSummary();
  status.replace("\n", "; ");
  demoLogging.log(BacnetDemoLogging::Level::Info, "watched AV read details: %s", readStatus.c_str());
  demoLogging.log(BacnetDemoLogging::Level::Info, "watched AV status details: %s", status.c_str());
}

static bool firstDiscoveredObject(const BacnetValueObjectPreview* objects,
                                  size_t count,
                                  BacnetObjectId& object,
                                  const BacnetScannedObject*& labelSource) {
  for (size_t i = 0; i < count; ++i) {
    if (!objects[i].discovered) {
      continue;
    }
    object = objects[i].object;
    labelSource = objects[i].scanned;
    return true;
  }
  return false;
}

static bool selectedStatusObject(BacnetObjectId& object,
                                 const BacnetScannedObject*& labelSource) {
  labelSource = nullptr;
  if (BACNET_STATUS_OBJECT_INSTANCE != 0) {
    object = BacnetObjectId{static_cast<uint16_t>(BACNET_STATUS_OBJECT_TYPE),
                            BACNET_STATUS_OBJECT_INSTANCE};
    return true;
  }

  if (firstDiscoveredObject(
        analogValues, kBacnetMaxFoundObjectsToDisplay, object, labelSource)) {
    return true;
  }
  if (firstDiscoveredObject(multiStateValues,
                            kBacnetMaxFoundObjectsToDisplay,
                            object,
                            labelSource)) {
    return true;
  }
  return firstDiscoveredObject(
    binaryValues, kBacnetMaxFoundObjectsToDisplay, object, labelSource);
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
  demoLogging.log(
    BacnetDemoLogging::Level::Info,
    "scan progress phase=%s index=%lu count=%lu found=%u stored=%u",
    bacnetObjectListScanPhaseText(progress.phase),
    static_cast<unsigned long>(progress.currentIndex),
    static_cast<unsigned long>(progress.summary.objectListCount),
    static_cast<unsigned>(progress.summary.found),
    static_cast<unsigned>(progress.summary.stored));
}

static void readBacnetObjectStatusPreview(BacnetDeviceSession& session) {
  BacnetObjectId object;
  const BacnetScannedObject* labelSource = nullptr;
  if (!selectedStatusObject(object, labelSource)) {
    bacnetStatusPreview = BacnetObjectStatusPreview{};
    bacnetStatusPreview.fallbackStatus = "No process object found";
    return;
  }

  BacnetObjectStatus status;
  session.readObjectStatus(object, status, kBacnetScanReadTimeoutMs, true);

  bacnetStatusPreview.available = true;
  bacnetStatusPreview.object = object;
  bacnetStatusPreview.labelSource = labelSource;
  bacnetStatusPreview.status = status;
}

static void appendStatusFlagsSummary(FixedTextBuffer& out,
                                     const BacnetObjectStatus& status) {
  if (status.statusFlagsStatus != BacnetPropertyReadStatus::Ack) {
    out.append(bacnetPropertyReadStatusText(status.statusFlagsStatus));
    return;
  }

  out.append(status.statusFlags.inAlarm ? "alarm" : "no-alarm");
  out.append(',');
  out.append(status.statusFlags.fault ? "fault" : "no-fault");
  out.append(',');
  out.append(status.statusFlags.overridden ? "overridden" : "normal");
  out.append(',');
  out.append(status.statusFlags.outOfService ? "oos" : "in-service");
}

static void appendEnumPropertySummary(FixedTextBuffer& out,
                                      BacnetPropertyReadStatus status,
                                      const char* text) {
  out.append(status == BacnetPropertyReadStatus::Ack
               ? (text != nullptr ? text : "")
               : bacnetPropertyReadStatusText(status));
}

static void appendBoolPropertySummary(FixedTextBuffer& out,
                                      BacnetPropertyReadStatus status,
                                      bool value) {
  out.append(status == BacnetPropertyReadStatus::Ack
               ? (value ? "true" : "false")
               : bacnetPropertyReadStatusText(status));
}

static void formatBacnetObjectStatusSummary(FixedTextBuffer& out) {
  if (!activeBacnetSession) {
    out.append("No device selected");
    return;
  }
  if (!bacnetStatusPreview.available) {
    out.append(bacnetScanStatus.length() ? bacnetScanStatus.c_str()
                                         : bacnetStatusPreview.fallbackStatus);
    return;
  }

  const BacnetObjectStatus& status = bacnetStatusPreview.status;
  appendObjectDisplayName(out, bacnetStatusPreview.object);
  out.append(": ");
  const char* label = bacnetStatusPreview.labelSource != nullptr
                        ? scannedLabelOrNull(*bacnetStatusPreview.labelSource)
                        : nullptr;
  if (label != nullptr && label[0] != '\0') {
    out.appendShortened(label);
  } else {
    appendObjectDisplayName(out, bacnetStatusPreview.object);
  }
  out.append("\nstate=");
  out.append(bacnetObjectHealthStateText(status.state));
  out.append(" pv=");
  out.append(status.presentValueStatus == BacnetPropertyReadStatus::Ack
               ? status.presentValue.displayText()
               : bacnetPropertyReadStatusText(status.presentValueStatus));
  out.append(" pv-status=");
  out.append(bacnetPropertyReadStatusText(status.presentValueStatus));
  out.append("\nflags=");
  appendStatusFlagsSummary(out, status);
  out.append(" event-state=");
  appendEnumPropertySummary(
    out, status.eventStateStatus, bacnetEventStateText(status.eventState));
  out.append("\nreliability=");
  appendEnumPropertySummary(
    out, status.reliabilityStatus, bacnetReliabilityText(status.reliability));
  out.append(" oos=");
  appendBoolPropertySummary(out, status.outOfServiceStatus, status.outOfService);
}

static void clearBacnetProcessObjectPreview(const char* status) {
  resetValueObjects(analogValues, kBacnetMaxFoundObjectsToDisplay);
  resetValueObjects(binaryValues, kBacnetMaxFoundObjectsToDisplay);
  resetValueObjects(multiStateValues, kBacnetMaxFoundObjectsToDisplay);
  bacnetStatusPreview = BacnetObjectStatusPreview{};
  bacnetStatusPreview.fallbackStatus = status;
  bacnetScanStoredObjects = 0;
}

static void requestBacnetRescan(const char* source) {
  const char* safeSource = source != nullptr ? source : "unknown";
  demoLogging.log(BacnetDemoLogging::Level::Info, "rescan requested source=%s", safeSource);

  if (!activeBacnetSession || !bacnetDeviceSelected) {
    demoLogging.log(BacnetDemoLogging::Level::Warn,
                    "rescan rejected source=%s reason=no device selected",
                    safeSource);
    return;
  }
  if (bacnetScanRunning || scanJob.isActive()) {
    demoLogging.log(BacnetDemoLogging::Level::Warn,
                    "rescan rejected source=%s reason=scan already running",
                    safeSource);
    return;
  }
  if (bacnetRescanPending || bacnetScanRequested) {
    demoLogging.log(BacnetDemoLogging::Level::Info,
                    "rescan accepted source=%s state=already queued",
                    safeSource);
    return;
  }

  bacnetRescanPending = true;
  bacnetRescanSource = safeSource;
  bacnetScanStatus = "Rescan queued";
  bacnetStatusPreview.available = false;
  bacnetStatusPreview.fallbackStatus = "Rescan queued";
  demoLogging.log(BacnetDemoLogging::Level::Info, "scan queued source=%s", safeSource);
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
    bacnetStatusPreview.fallbackStatus = "Rescan rejected: no device selected";
    demoLogging.log(BacnetDemoLogging::Level::Warn,
                    "rescan rejected source=%s reason=no device selected",
                    source.c_str());
    return;
  }
  if (bacnetScanRunning || scanJob.isActive()) {
    demoLogging.log(BacnetDemoLogging::Level::Warn,
                    "rescan rejected source=%s reason=scan already running",
                    source.c_str());
    return;
  }

  clearBacnetProcessObjectPreview("Rescanning");
  bacnetScanStatus = "Rescanning";
  bacnetScanRequested = true;
  bacnetScanFinished = false;
  demoLogging.log(BacnetDemoLogging::Level::Info, "rescan accepted source=%s scan queued", source.c_str());
}

static void addRuntimeTextField(const char* sourceGroup,
                                const char* key,
                                const char* label,
                                const char* page,
                                const char* card,
                                const char* group,
                                int order,
                                bool multiline = false) {
  RuntimeFieldMeta meta;
  meta.sourceGroup = sourceGroup;
  meta.key = key;
  meta.label = label != nullptr ? label : key;
  meta.page = page;
  meta.card = card;
  meta.group = group;
  meta.order = order;
  meta.isString = true;
  if (multiline) {
    meta.style.rule("row").addCSSClass("bacnetObjectListValue");
    meta.style.rule("label").addCSSClass("bacnetObjectListValue");
    meta.style.rule("values").addCSSClass("bacnetObjectListValue");
    meta.style.rule("unit").addCSSClass("bacnetObjectListValue");
  }
  ConfigManager.getRuntime().addRuntimeMeta(meta);
}

static void fillBacnetRuntime(JsonObject& data) {
  char device[96] = {};
  FixedTextBuffer deviceOut(device, sizeof(device));
  formatBacnetDeviceSummary(deviceOut);
  data["device0"] = deviceOut.data;

  for (size_t propertyIndex = 0; propertyIndex < kBacnetPreviewPropertyCount;
       ++propertyIndex) {
    char value[BacnetValue::kMaxTextLength] = {};
    FixedTextBuffer valueOut(value, sizeof(value));
    formatBacnetPropertySummary(propertyIndex, valueOut);
    char key[40] = {};
    std::snprintf(key,
                  sizeof(key),
                  "device0_%s",
                  kBacnetPreviewProperties[propertyIndex].name);
    data[key] = valueOut.data;
  }

  char listText[(BacnetValue::kMaxTextLength + 128) *
                kBacnetMaxFoundObjectsToDisplay] = {};
  FixedTextBuffer listOut(listText, sizeof(listText));
  formatBacnetValueObjectsSummary(analogValues,
                                  kBacnetMaxFoundObjectsToDisplay,
                                  bacnetScanFinished,
                                  "analog",
                                  listOut);
  data["device0_analogValues"] = listOut.data;

  listText[0] = '\0';
  listOut = FixedTextBuffer(listText, sizeof(listText));
  formatBacnetValueObjectsSummary(binaryValues,
                                  kBacnetMaxFoundObjectsToDisplay,
                                  bacnetScanFinished,
                                  "binary",
                                  listOut);
  data["device0_binaryValues"] = listOut.data;

  listText[0] = '\0';
  listOut = FixedTextBuffer(listText, sizeof(listText));
  formatBacnetValueObjectsSummary(multiStateValues,
                                  kBacnetMaxFoundObjectsToDisplay,
                                  bacnetScanFinished,
                                  "multi-state",
                                  listOut);
  data["device0_multiStateValues"] = listOut.data;

  char health[BacnetValue::kMaxTextLength + 256] = {};
  FixedTextBuffer healthOut(health, sizeof(health));
  formatBacnetObjectStatusSummary(healthOut);
  data["device0_objectHealth"] = healthOut.data;
}

static void fillWatchedAnalogRuntime(JsonObject& data) {
  data["watchedAv_object"] = watchedAnalogValue.objectSummary();
  data["watchedAv_label"] = watchedAnalogValue.labelSummary();
  data["watchedAv_description"] = watchedAnalogValue.descriptionSummary();
  data["watchedAv_value"] = watchedAnalogValue.valueSummary();
  data["watchedAv_metadata"] = watchedAnalogValue.metadataSummary();
  data["watchedAv_alarm"] = watchedAnalogValue.alarmStateSummary();
  data["watchedAv_refresh"] = watchedAnalogValue.refreshSummary();
}

static void setupRuntimeUI() {
  auto bacnetDeviceGroup = ConfigManager.liveGroup("bacnet")
                             .page("Sensors", 10)
                             .card("BACnet/IP Client", 20)
                             .group("Selected Device", 1);

  ConfigManager.getRuntime().addRuntimeProvider("bacnet", fillBacnetRuntime, 20);
  addRuntimeTextField("bacnet",
                      "device0",
                      "Device",
                      "Sensors",
                      "BACnet/IP Client",
                      "Selected Device",
                      10);

  bacnetDeviceGroup.button("device0_rescan", "Scan / Rescan", []() { requestBacnetRescan("ui"); })
    .order(15);

  for (size_t propertyIndex = 0; propertyIndex < kBacnetPreviewPropertyCount;
       ++propertyIndex) {
    char propertyKey[40] = {};
    std::snprintf(propertyKey,
                  sizeof(propertyKey),
                  "device0_%s",
                  kBacnetPreviewProperties[propertyIndex].name);
    addRuntimeTextField("bacnet",
                        propertyKey,
                        kBacnetPreviewProperties[propertyIndex].name,
                        "Sensors",
                        "BACnet/IP Client",
                        "Selected Device",
                        20 + propertyIndex);
  }

  bacnetDeviceGroup.divider("Analog Objects", 39);
  addRuntimeTextField("bacnet",
                      "device0_analogValues",
                      "Analog",
                      "Sensors",
                      "BACnet/IP Client",
                      "Selected Device",
                      40,
                      true);

  bacnetDeviceGroup.divider("Binary Objects", 49);
  addRuntimeTextField("bacnet",
                      "device0_binaryValues",
                      "Binary",
                      "Sensors",
                      "BACnet/IP Client",
                      "Selected Device",
                      50,
                      true);

  bacnetDeviceGroup.divider("Multi-State Objects", 59);
  addRuntimeTextField("bacnet",
                      "device0_multiStateValues",
                      "Multi-State",
                      "Sensors",
                      "BACnet/IP Client",
                      "Selected Device",
                      60,
                      true);

  bacnetDeviceGroup.divider("Object Health", 69);
  addRuntimeTextField("bacnet",
                      "device0_objectHealth",
                      "State",
                      "Sensors",
                      "BACnet/IP Client",
                      "Selected Device",
                      70,
                      true);

  auto watchedAnalogGroup = ConfigManager.liveGroup("bacnetWatch")
                              .page("Sensors", 10)
                              .card("Watched Analog Value", 30)
                              .group("AV Watch", 1);

  ConfigManager.getRuntime().addRuntimeProvider(
    "bacnetWatch", fillWatchedAnalogRuntime, 30);
  addRuntimeTextField("bacnetWatch",
                      "watchedAv_object",
                      "Object",
                      "Sensors",
                      "Watched Analog Value",
                      "AV Watch",
                      10);
  addRuntimeTextField("bacnetWatch",
                      "watchedAv_label",
                      "Name",
                      "Sensors",
                      "Watched Analog Value",
                      "AV Watch",
                      20);
  addRuntimeTextField("bacnetWatch",
                      "watchedAv_description",
                      "Description",
                      "Sensors",
                      "Watched Analog Value",
                      "AV Watch",
                      25);
  addRuntimeTextField("bacnetWatch",
                      "watchedAv_value",
                      "Present Value",
                      "Sensors",
                      "Watched Analog Value",
                      "AV Watch",
                      30);
  addRuntimeTextField("bacnetWatch",
                      "watchedAv_metadata",
                      "Metadata",
                      "Sensors",
                      "Watched Analog Value",
                      "AV Watch",
                      35);
  addRuntimeTextField("bacnetWatch",
                      "watchedAv_alarm",
                      "Alarm State",
                      "Sensors",
                      "Watched Analog Value",
                      "AV Watch",
                      39);
  addRuntimeTextField("bacnetWatch",
                      "watchedAv_refresh",
                      "Refresh",
                      "Sensors",
                      "Watched Analog Value",
                      "AV Watch",
                      45);

  watchedAnalogGroup
    .button("watchedAv_details", "Log to Serial", []() { logWatchedAnalogDetails(); })
    .order(50);
}

static void sendWhoIs() {
  if (!bacnetStarted || !bacnetAddressConfigValid) {
    return;
  }

  if (!bacnetClient.sendWhoIs(whoIsDestination)) {
    Serial.println("[W] BACnet Who-Is send failed");
    demoLogging.log(BacnetDemoLogging::Level::Warn, "Who-Is send failed");
    return;
  }

  lastWhoIsAt = millis();
}

static void readDeviceProperties(BacnetDeviceSession& session) {
  for (size_t i = 0; i < kBacnetPreviewPropertyCount; ++i) {
    BacnetPropertyPreview& property = bacnetDeviceProperties[i];
    property.status =
      session.object(session.deviceObject())
        .readProperty(property.id, property.value, kBacnetScanReadTimeoutMs);
  }
}

static void onPresentValueUpdate(
  const BacnetSubscriptionNotification& notification) {
  auto* preview =
    static_cast<BacnetValueObjectPreview*>(notification.userData);
  if (preview == nullptr) {
    return;
  }

  preview->presentValueStatus = notification.status;
  char objectName[24] = {};
  FixedTextBuffer objectOut(objectName, sizeof(objectName));
  appendObjectDisplayName(objectOut, notification.objectId);
  demoLogging.log(BacnetDemoLogging::Level::Info,
                  "subscription update %s present-value %s=%s",
                  objectOut.c_str(),
                  bacnetSubscriptionReasonText(notification),
                  valueObjectPresentValueText(*preview) != nullptr
                    ? valueObjectPresentValueText(*preview)
                    : bacnetReadStatusText(preview->presentValueStatus));
}

static bool subscribePresentValue(BacnetDeviceSession& session,
                                  BacnetValueObjectPreview& preview) {
  if (!preview.discovered) {
    return false;
  }

  BacnetSubscribeOptions options;
  options.fallbackPollMs = kBacnetSubscriptionFallbackPollMs;
  options.timeoutMs = kBacnetScanReadTimeoutMs;
  options.immediateFirstRead = false;
  options.notifyOnStatusChange = true;

  preview.subscription.reset(new BacnetPropertySubscription(
    session.object(preview.object)
      .property(BacnetPropertyId::PresentValue)
      .subscribe(onPresentValueUpdate, &preview, options)));
  demoLogging.log(BacnetDemoLogging::Level::Info,
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
    destination[i].scanned = &scanned;
    destination[i].presentValueStatus = scanned.presentValueStatus;
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
    if (bacnetIsAnalogProcessObject(scanBuffer[i].objectId.type)) {
      if (copyScannedObjectToPreview(scanBuffer[i], analogValues, kBacnetMaxFoundObjectsToDisplay)) {
        ++analogStored;
      }
    } else if (bacnetIsBinaryProcessObject(scanBuffer[i].objectId.type)) {
      if (copyScannedObjectToPreview(scanBuffer[i], binaryValues, kBacnetMaxFoundObjectsToDisplay)) {
        ++binaryStored;
      }
    } else if (bacnetIsMultiStateProcessObject(scanBuffer[i].objectId.type)) {
      if (copyScannedObjectToPreview(scanBuffer[i], multiStateValues, kBacnetMaxFoundObjectsToDisplay)) {
        ++multiStateStored;
      }
    }
  }
}

static bool readConfiguredFallbackObject(BacnetDeviceSession& session,
                                         BacnetObjectId object,
                                         BacnetScannedObject& scanned,
                                         BacnetValueObjectPreview* destination,
                                         size_t destinationCount) {
  if (object.instance == 0) {
    return false;
  }

  scanned = BacnetScannedObject{};
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

  demoLogging.log(BacnetDemoLogging::Level::Info,
                  "fallback probe %s objectName=%s description=%s present-value=%s",
                  bacnetObjectDisplayName(object).c_str(),
                  bacnetReadStatusText(scanned.objectNameStatus),
                  bacnetReadStatusText(scanned.descriptionStatus),
                  bacnetReadStatusText(scanned.presentValueStatus));

  if (scanned.objectNameStatus != BacnetDeviceSessionReadStatus::Ack &&
      scanned.descriptionStatus != BacnetDeviceSessionReadStatus::Ack &&
      scanned.presentValueStatus != BacnetDeviceSessionReadStatus::Ack) {
    demoLogging.log(BacnetDemoLogging::Level::Warn, "fallback object %s unavailable", bacnetObjectDisplayName(object).c_str());
    return false;
  }

  if (!copyScannedObjectToPreview(scanned, destination, destinationCount)) {
    demoLogging.log(BacnetDemoLogging::Level::Warn, "fallback object %s not stored: display full", bacnetObjectDisplayName(object).c_str());
    return false;
  }
  return true;
}

static size_t appendConfiguredFallbackObjects(BacnetDeviceSession& session,
                                              size_t& analogStored,
                                              size_t& binaryStored,
                                              size_t& multiStateStored) {
  size_t loaded = 0;
  size_t scanSlot = 0;
  demoLogging.log(BacnetDemoLogging::Level::Info, "configured fallback probe started");

  if (readConfiguredFallbackObject(
        session,
        bacnetDemoFallbackAnalogObject(BACNET_FALLBACK_ANALOG_OBJECT_INSTANCE),
        scanBuffer[scanSlot],
        analogValues,
        kBacnetMaxFoundObjectsToDisplay)) {
    ++analogStored;
    ++loaded;
  }
  ++scanSlot;
  if (readConfiguredFallbackObject(
        session,
        bacnetDemoFallbackBinaryObject(BACNET_FALLBACK_BINARY_OBJECT_INSTANCE),
        scanBuffer[scanSlot],
        binaryValues,
        kBacnetMaxFoundObjectsToDisplay)) {
    ++binaryStored;
    ++loaded;
  }
  ++scanSlot;
  if (readConfiguredFallbackObject(
        session,
        bacnetDemoFallbackMultiStateObject(
          BACNET_FALLBACK_MULTISTATE_OBJECT_INSTANCE),
        scanBuffer[scanSlot],
        multiStateValues,
        kBacnetMaxFoundObjectsToDisplay)) {
    ++multiStateStored;
    ++loaded;
  }

  demoLogging.log(BacnetDemoLogging::Level::Info, "configured fallback probe complete loaded=%u", static_cast<unsigned>(loaded));
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
  demoLogging.log(BacnetDemoLogging::Level::Info,
                  "scan start requested maxEntries=%lu capacity=%u filter=%u",
                  static_cast<unsigned long>(options.maxObjectListEntries),
                  static_cast<unsigned>(kBacnetScanResultCapacity),
                  static_cast<unsigned>(sizeof(kValueObjectScanFilter) /
                                        sizeof(kValueObjectScanFilter[0])));
  if (!session.beginObjectListScan(scanJob, options, scanBuffer, kBacnetScanResultCapacity)) {
    bacnetScanStatus = "Scan start failed: ";
    bacnetScanStatus += bacnetObjectListScanJobStatusText(scanJob.status());
    demoLogging.log(BacnetDemoLogging::Level::Warn,
                    "scan start failed status=%s phase=%s count-status=%s",
                    bacnetObjectListScanJobStatusText(scanJob.status()),
                    bacnetObjectListScanPhaseText(scanJob.phase()),
                    bacnetReadStatusText(scanJob.summary().objectListCountStatus));
    return false;
  }
  bacnetScanStatus = "Object-list scan running";
  demoLogging.log(BacnetDemoLogging::Level::Info, "object-list count read started");
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
  watchedAnalogValue.setup(session);

  bacnetScanFinished = true;
  bacnetScanRunning = false;
  demoLogging.log(BacnetDemoLogging::Level::Info,
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
    demoLogging.log(BacnetDemoLogging::Level::Info,
                    "using configured fallback objects loaded=%u total=%u",
                    static_cast<unsigned>(fallbackStored),
                    static_cast<unsigned>(bacnetScanStoredObjects));
  } else if (scan.stored == 0 ||
             scan.objectListCountStatus != BacnetDeviceSessionReadStatus::Ack) {
    demoLogging.log(BacnetDemoLogging::Level::Warn, "scan result: %s", bacnetScanStatus.c_str());
  }
  demoLogging.log(BacnetDemoLogging::Level::Info, "subscriptions recreated count=%u", static_cast<unsigned>(subscriptionsCreated));
}

static void scanSelectedBacnetDevice() {
  if (!activeBacnetSession || bacnetScanFinished) {
    return;
  }

  if (bacnetScanRequested && !bacnetScanRunning) {
    bacnetScanRequested = false;
    bacnetScanRunning = true;
    bacnetScanStatus = "Reading Device properties";
    demoLogging.log(
      BacnetDemoLogging::Level::Info, "scanning selected device %lu", static_cast<unsigned long>(activeBacnetSession->deviceInstance()));
    demoLogging.log(BacnetDemoLogging::Level::Info, "device metadata read started");

    readDeviceProperties(*activeBacnetSession);
    demoLogging.log(BacnetDemoLogging::Level::Info, "device metadata read complete");
    if (!beginValueObjectScan(*activeBacnetSession)) {
      bacnetScanRunning = false;
      bacnetScanFinished = true;
      readBacnetObjectStatusPreview(*activeBacnetSession);
      watchedAnalogValue.setup(*activeBacnetSession);
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
    BacnetDeviceSession::fromEndpoint(bacnetClient, deviceInstance, address, port)));
  bacnetDeviceSelected = true;
  bacnetScanStatus = "Scan queued for selected device";

  Serial.print("[I] BACnet selected device ");
  Serial.print(deviceInstance);
  Serial.print(" at ");
  Serial.print(address);
  Serial.print(":");
  Serial.println(port);
  demoLogging.log(BacnetDemoLogging::Level::Info, "selected device %lu at %s:%u", static_cast<unsigned long>(deviceInstance), address.toString().c_str(), static_cast<unsigned>(port));

  bacnetScanRequested = true;
  demoLogging.log(BacnetDemoLogging::Level::Info, "scan queued for selected device %lu", static_cast<unsigned long>(deviceInstance));
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
  demoLogging.log(BacnetDemoLogging::Level::Info, "selected device %lu at %s:%u", static_cast<unsigned long>(device.deviceInstance), device.address.toString().c_str(), static_cast<unsigned>(BacnetClient::kDefaultPort));

  bacnetScanRequested = true;
  demoLogging.log(BacnetDemoLogging::Level::Info, "scan queued for selected device %lu", static_cast<unsigned long>(device.deviceInstance));
}

static void clearBacnetRuntime() {
  if (activeBacnetSession && scanJob.isActive()) {
    activeBacnetSession->cancelObjectListScan(scanJob);
  }
  resetValueObjects(analogValues, kBacnetMaxFoundObjectsToDisplay);
  resetValueObjects(binaryValues, kBacnetMaxFoundObjectsToDisplay);
  resetValueObjects(multiStateValues, kBacnetMaxFoundObjectsToDisplay);
  bacnetStatusPreview = BacnetObjectStatusPreview{};
  watchedAnalogValue.reset("not read");
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
      demoLogging.log(BacnetDemoLogging::Level::Error, "disabled: invalid BACnet IP config");
    }
    return;
  }

  if (!bacnetClient.begin()) {
    Serial.println("[E] BACnet UDP listener failed to start");
    demoLogging.log(BacnetDemoLogging::Level::Error, "client UDP listener failed");
    return;
  }

  bacnetStarted = true;
  Serial.print("[I] BACnet client started on UDP port ");
  Serial.println(bacnetClient.localPort());

  sendWhoIs();
  selectBacnetDevice(BACNET_TARGET_DEVICE_INSTANCE,
                     configuredBacnetTargetAddress,
                     BACNET_TARGET_PORT,
                     0);
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

  watchedAnalogValue.poll(*activeBacnetSession, now);
}

void setup() {
  Serial.begin(115200);
  Serial.println("[I] BACnet client demo starting");

  ConfigManagerClass::setLogger([](const char* msg) {
    Serial.print("[D] CM ");
    Serial.println(msg);
  });

  resetBacnetPreviews();
  watchedAnalogValue.setLogger(demoLogging.watchedAnalogCallback());
  ConfigManager.setAppName(APP_NAME);
  ConfigManager.setAppTitle(APP_NAME);
  ConfigManager.setVersion(APP_VERSION);
  ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);
  ConfigManager.enableBuiltinSystemProvider();
  ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE,
                             sizeof(GLOBAL_THEME_OVERRIDE) - 1);
  demoLogging.setup();
  configureBacnetAddresses();

  coreSettings.attachWiFi(ConfigManager);
  coreSettings.attachSystem(ConfigManager);
  coreSettings.attachNtp(ConfigManager);

  ConfigManager.loadAll();
  setupNetworkDefaults();

  setupRuntimeUI();

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
  demoLogging.loop();
  bacnetClient.logger().tick();
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
