// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <Arduino.h>

#include <ArduinoBacnetClient.h>
#include <BacnetClient.h>
#include <BacnetDeviceSession.h>
#include <BacnetDisplayText.h>
#include <BacnetRemoteObject.h>
#include <WiFiUdp.h>
#ifndef BACNET_DEMO_USE_ETHERNET
#define BACNET_DEMO_USE_ETHERNET 0
#endif

#if BACNET_DEMO_USE_ETHERNET
#include <ETH.h>
#include <ExampleEthernet.h>
#else
#include <WiFi.h>
#endif

#include "ConfigManager.h"
#include "core/CoreSettings.h"
#if !BACNET_DEMO_USE_ETHERNET
#include "core/CoreWiFiServices.h"
#endif

#include "BacnetDemoBinaryValueStatus.h"
#include "BacnetDemoFormat.h"
#include "BacnetDemoLogging.h"

#ifndef BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
#define BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS 1
#endif
#include "BacnetDemoPropertyBrowser.h"
#include "BacnetDemoWatchedAnalogValue.h"

#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#define BACNET_DEMO_HAS_SECRETS 1
#else
#define BACNET_DEMO_HAS_SECRETS 0
#endif

#ifndef APP_VERSION
#define APP_VERSION "0.34.0"
#endif
#ifndef APP_NAME
#if BACNET_DEMO_USE_ETHERNET
#define APP_NAME "BACnet Client Demo ETH"
#else
#define APP_NAME "BACnet Client Demo"
#endif
#endif

#ifndef SETTINGS_PASSWORD
#define SETTINGS_PASSWORD ""
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD SETTINGS_PASSWORD
#endif

#if BACNET_DEMO_USE_ETHERNET
#ifndef MY_ETHERNET_IP
#define MY_ETHERNET_IP "192.168.2.127"
#endif
#ifndef MY_GATEWAY_IP
#define MY_GATEWAY_IP "192.168.2.1"
#endif
#ifndef MY_SUBNET_MASK
#define MY_SUBNET_MASK "255.255.255.0"
#endif
#ifndef MY_DNS_IP
#define MY_DNS_IP MY_GATEWAY_IP
#endif
#ifndef MY_USE_DHCP
#define MY_USE_DHCP false
#endif
#endif

#ifndef BACNET_DEMO_DEFAULT_COV_LIFETIME_SECONDS
#define BACNET_DEMO_DEFAULT_COV_LIFETIME_SECONDS 120
#endif

#ifndef BACNET_DEMO_OBJECT_COV_LIFETIME_SECONDS
#define BACNET_DEMO_OBJECT_COV_LIFETIME_SECONDS 0
#endif

extern ConfigManagerClass ConfigManager;

static cm::CoreSettings& coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings& systemSettings = coreSettings.system;
static cm::CoreNtpSettings& ntpSettings = coreSettings.ntp;
static Config<int> bacnetWritePriority{ConfigOptions<int>{.key = "BacnetWritePriority", .name = "Write Priority", .category = "BACnet Override", .defaultValue = 8, .showInWeb = true, .sortOrder = 10}};
static Config<int> bacnetOverrideAvInstance{ConfigOptions<int>{.key = "BacnetOverrideAV", .name = "AV Override Instance", .category = "BACnet Override", .defaultValue = 200, .showInWeb = true, .sortOrder = 20}};
static Config<int> bacnetPollingAvInstance{ConfigOptions<int>{.key = "BacnetPollingAV", .name = "AV Polling Instance", .category = "BACnet Override", .defaultValue = 201, .showInWeb = true, .sortOrder = 30}};
static Config<int> bacnetOverrideBvInstance{ConfigOptions<int>{.key = "BacnetOverrideBV", .name = "BV Override Instance", .category = "BACnet Override", .defaultValue = 320, .showInWeb = true, .sortOrder = 40}};
static Config<int> bacnetCovLifetime{ConfigOptions<int>{.key = "BacnetCovLifetime", .name = "COV Lifetime Seconds", .category = "BACnet Override", .defaultValue = BACNET_DEMO_DEFAULT_COV_LIFETIME_SECONDS, .showInWeb = true, .sortOrder = 50}};
static Config<int> bacnetBrowserObjectType{ConfigOptions<int>{.key = "BacnetBrowserObjectType", .name = "Object Type", .category = "BACnet Property Browser", .defaultValue = static_cast<int>(BacnetObjectType::Device), .showInWeb = true, .sortOrder = 10}};
static Config<int> bacnetBrowserObjectInstance{ConfigOptions<int>{.key = "BacnetBrowserObjectInstance", .name = "Object Instance", .category = "BACnet Property Browser", .defaultValue = 0, .showInWeb = true, .sortOrder = 20}};
static Config<int> bacnetBrowserPropertyRow{ConfigOptions<int>{.key = "BacnetBrowserPropertyRow", .name = "Property Row (1..8)", .category = "BACnet Property Browser", .defaultValue = 1, .showInWeb = true, .sortOrder = 30}};
#if BACNET_DEMO_USE_ETHERNET
static Config<String> ethernetIp{ConfigOptions<String>{.key = "EthIP", .name = "IP Address", .category = "Ethernet", .defaultValue = String(""), .showInWeb = true, .sortOrder = 1}};
static Config<String> ethernetSubnet{ConfigOptions<String>{.key = "EthSubnet", .name = "Subnet Mask", .category = "Ethernet", .defaultValue = String(""), .showInWeb = true, .sortOrder = 2}};
static Config<String> ethernetGateway{ConfigOptions<String>{.key = "EthGateway", .name = "Gateway", .category = "Ethernet", .defaultValue = String(""), .showInWeb = true, .sortOrder = 3}};
static Config<String> ethernetDns{ConfigOptions<String>{.key = "EthDNS", .name = "Primary DNS", .category = "Ethernet", .defaultValue = String(""), .showInWeb = true, .sortOrder = 4}};
static Config<String> settingsPassword{ConfigOptions<String>{.key = "SettingsPass", .name = "Settings Password", .category = "System", .defaultValue = String(""), .showInWeb = true, .isPassword = true, .sortOrder = 2}};
static bool ethernetWasConnected = false;
static bool ethernetServicesStarted = false;
#else
static cm::CoreWiFiSettings& wifiSettings = coreSettings.wifi;
static cm::CoreWiFiServices wifiServices;
#endif

static WiFiUDP bacnetUdp;
static ArduinoUdpDatagramTransport bacnetTransport(bacnetUdp);
static ArduinoMonotonicClock bacnetClock;
static BacnetClient bacnetClient(bacnetTransport, &bacnetClock);
static BacnetDemoLogging demoLogging(ConfigManager, bacnetClient);

#ifndef BACNET_WHOIS_DESTINATION
#if BACNET_DEMO_USE_ETHERNET
#define BACNET_WHOIS_DESTINATION "192.168.2.255"
#else
#define BACNET_WHOIS_DESTINATION "192.0.2.255"
#endif
#endif

#ifndef BACNET_TARGET_ADDRESS
#if BACNET_DEMO_USE_ETHERNET
#define BACNET_TARGET_ADDRESS "192.168.2.101"
#else
#define BACNET_TARGET_ADDRESS "192.0.2.101"
#endif
#endif

#ifndef BACNET_TARGET_DEVICE_INSTANCE
#define BACNET_TARGET_DEVICE_INSTANCE 1682101
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

#ifndef BACNET_OVERRIDE_BINARY_OBJECT_INSTANCE
#define BACNET_OVERRIDE_BINARY_OBJECT_INSTANCE 320
#endif

#ifndef BACNET_WATCHED_ANALOG_VALUE_INSTANCE
#define BACNET_WATCHED_ANALOG_VALUE_INSTANCE 200
#endif

#ifndef CLIENT_DEMO_ENABLE_WATCH_METADATA_FIELD
#define CLIENT_DEMO_ENABLE_WATCH_METADATA_FIELD 0
#endif

#ifndef BACNET_DEMO_ENABLE_WATCHED_VALUES
#define BACNET_DEMO_ENABLE_WATCHED_VALUES 1
#endif

#ifndef BACNET_DEMO_ENABLE_OBJECT_COV
#define BACNET_DEMO_ENABLE_OBJECT_COV 0
#endif

#ifndef BACNET_DEMO_ENABLE_RUNTIME_GUI
#define BACNET_DEMO_ENABLE_RUNTIME_GUI 1
#endif

#ifndef BACNET_DEMO_MAX_FOUND_OBJECTS_TO_DISPLAY
#define BACNET_DEMO_MAX_FOUND_OBJECTS_TO_DISPLAY 3
#endif

static constexpr uint32_t kWhoIsIntervalMs = 30000;
static constexpr uint32_t kBacnetScanReadTimeoutMs = 3000;
static constexpr uint32_t kBacnetMaxObjectListEntriesToInspect = 600;
static constexpr size_t kBacnetMaxFoundObjectsToDisplay =
  BACNET_DEMO_MAX_FOUND_OBJECTS_TO_DISPLAY;
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

static uint32_t processObjectCovLifetimeSeconds() {
#if BACNET_DEMO_OBJECT_COV_LIFETIME_SECONDS > 0
  return BACNET_DEMO_OBJECT_COV_LIFETIME_SECONDS;
#else
  return static_cast<uint32_t>(bacnetCovLifetime.get());
#endif
}

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
  BacnetValue statusFlags;
  BacnetPropertyReadStatus statusFlagsStatus =
    BacnetPropertyReadStatus::Skipped;
  uint32_t covUpdateCount = 0;
  uint32_t lastCovUpdateMs = 0;
  bool usesPropertyCov = false;
  std::unique_ptr<BacnetPropertySubscription> subscription;
  std::unique_ptr<BacnetPropertySubscription> statusSubscription;
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
  kBacnetScanReadTimeoutMs,
  true);
static BacnetDemoWatchedAnalogValue polledAnalogValue(
  BACNET_WATCHED_ANALOG_VALUE_INSTANCE + 1,
  kWatchedAnalogValuePollMs,
  kBacnetScanReadTimeoutMs);
static BacnetDemoBinaryValueStatus overrideBinaryValueStatus(
  BACNET_OVERRIDE_BINARY_OBJECT_INSTANCE,
  kWatchedAnalogValuePollMs,
  kBacnetScanReadTimeoutMs);
static float bacnetOverrideAnalogInput = 0.0F;
static String bacnetOverrideStatus = "No manual action yet";
static BacnetDemoPropertyBrowser propertyBrowser;
static String propertyBrowserStatus = "Load a scanned object on demand";
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

#if !BACNET_DEMO_USE_ETHERNET
void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();
#endif
static void setupNetworkDefaults();

static bool parseBacnetAddress(const char* label,
                               const char* addressText,
                               IPAddress& output) {
  if (addressText != nullptr && output.fromString(addressText)) {
    return true;
  }

#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
  Serial.print("[E] Invalid BACnet ");
  Serial.print(label);
  Serial.print(" IP: ");
  Serial.println(addressText != nullptr ? addressText : "<null>");
#endif
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
  polledAnalogValue.reset("not read");
  overrideBinaryValueStatus.reset();
  propertyBrowser.reset(activeBacnetSession.get());
  propertyBrowserStatus = "Load a scanned object on demand";
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

static bool isDisplayedBacnetObject(BacnetObjectId object) {
  if (!activeBacnetSession) {
    return false;
  }
  if (object.type == static_cast<uint16_t>(BacnetObjectType::Device) &&
      object.instance == activeBacnetSession->deviceInstance()) {
    return true;
  }
  // The two bounded watch objects are intentionally available to the browser
  // as well.  They are configured demo inputs, not arbitrary user-supplied
  // object IDs, and remain useful when a device does not implement ObjectList.
  if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogValue) &&
      (object.instance == BACNET_WATCHED_ANALOG_VALUE_INSTANCE ||
       object.instance == BACNET_WATCHED_ANALOG_VALUE_INSTANCE + 1)) {
    return true;
  }
  const BacnetValueObjectPreview* groups[] = {
    analogValues, binaryValues, multiStateValues};
  for (const BacnetValueObjectPreview* group : groups) {
    if (std::any_of(group,
                    group + kBacnetMaxFoundObjectsToDisplay,
                    [object](const BacnetValueObjectPreview& preview) {
                      return preview.discovered && preview.object.type == object.type &&
                             preview.object.instance == object.instance;
                    })) {
      return true;
    }
  }
  return false;
}

static bool configuredPropertyBrowserObject(BacnetObjectId& object) {
  if (!activeBacnetSession) {
    propertyBrowserStatus = "No device selected";
    return false;
  }
  const int configuredType = bacnetBrowserObjectType.get();
  const int configuredInstance = bacnetBrowserObjectInstance.get();
  if (configuredType < 0 || configuredType > 1023 ||
      configuredInstance < 0 || configuredInstance > 0x3FFFFF) {
    propertyBrowserStatus = "Invalid object type or instance";
    return false;
  }
  if (configuredType == static_cast<int>(BacnetObjectType::Device)) {
    object = activeBacnetSession->deviceObject();
  } else {
    object = BacnetObjectId{static_cast<uint16_t>(configuredType),
                            static_cast<uint32_t>(configuredInstance)};
  }
  if (!isDisplayedBacnetObject(object)) {
    // The preview is deliberately bounded, while the on-demand browser reads
    // exactly one explicitly configured object. Do not turn the UI/RAM limit
    // into a false claim that a known object is unavailable.
    demoLogging.log(BacnetDemoLogging::Level::Info,
                    "manual property browser object %s is outside preview",
                    bacnetObjectDisplayName(object).c_str());
  }
  return true;
}

static void loadPropertyBrowser() {
  if (bacnetScanRunning) {
    propertyBrowserStatus = "Scan is running";
    return;
  }
  BacnetObjectId object;
  if (!configuredPropertyBrowserObject(object)) {
    return;
  }
  propertyBrowser.stopSubscription();
  if (!propertyBrowser.queueLoad(object, kBacnetScanReadTimeoutMs)) {
    propertyBrowserStatus = "Property browser load already active";
    return;
  }
  propertyBrowserStatus = "Load queued";
}

static void updatePropertyBrowserStatus() {
  const BacnetPropertyReadAllResult& summary = propertyBrowser.result();
  if (propertyBrowser.loading()) {
    propertyBrowserStatus = propertyBrowser.loadStateText();
    return;
  }
  if (propertyBrowser.loadState() == BacnetDemoPropertyBrowser::LoadState::Failed) {
    propertyBrowserStatus = "Property-list: ";
    propertyBrowserStatus +=
      bacnetPropertyReadStatusText(summary.propertyListStatus);
    return;
  }
  if (propertyBrowser.loadState() == BacnetDemoPropertyBrowser::LoadState::Cancelled) {
    propertyBrowserStatus = "Load cancelled";
    return;
  }
  if (!propertyBrowser.loaded()) {
    propertyBrowserStatus = "Load a scanned object on demand";
    return;
  }
  if (propertyBrowser.usingFallbackProperties()) {
    propertyBrowserStatus = "Loaded bounded known properties: ";
  } else {
    propertyBrowserStatus = "Loaded ";
  }
  propertyBrowserStatus += String(static_cast<unsigned>(propertyBrowser.rowCount()));
  if (!propertyBrowser.usingFallbackProperties()) {
    propertyBrowserStatus += " of ";
    propertyBrowserStatus += String(static_cast<unsigned long>(summary.advertised));
    propertyBrowserStatus += " advertised properties";
  }
  if (summary.truncated) {
    propertyBrowserStatus += " (bounded)";
  }
}

static void subscribeSelectedPropertyBrowserProperty() {
  if (!activeBacnetSession || !propertyBrowser.loaded()) {
    propertyBrowserStatus = "Load properties before subscribing";
    return;
  }
  const int configuredRow = bacnetBrowserPropertyRow.get();
  if (configuredRow < 1 ||
      configuredRow > static_cast<int>(BacnetDemoPropertyBrowser::kMaxProperties) ||
      !propertyBrowser.select(static_cast<size_t>(configuredRow - 1))) {
    propertyBrowserStatus = "Invalid or unavailable property row";
    return;
  }
  const int configuredLifetime = bacnetCovLifetime.get();
  if (configuredLifetime < 1) {
    propertyBrowserStatus = "Invalid COV lifetime";
    return;
  }
  BacnetSubscribeOptions options;
  options.preferCov = true;
  options.covLifetimeSeconds = static_cast<uint32_t>(configuredLifetime);
  options.fallbackPollMs = kBacnetSubscriptionFallbackPollMs;
  options.timeoutMs = kBacnetScanReadTimeoutMs;
  options.immediateFirstRead = true;
  options.notifyOnStatusChange = true;
  if (!propertyBrowser.subscribe(*activeBacnetSession, options)) {
    propertyBrowserStatus = "Subscription creation failed";
    return;
  }
  const BacnetPropertyReadResult* row = propertyBrowser.selectedRow();
  propertyBrowserStatus = "Subscription: ";
  propertyBrowserStatus +=
    row != nullptr ? bacnetPropertyName(row->propertyId) : "unknown";
}

static void stopPropertyBrowserSubscription() {
  propertyBrowser.stopSubscription();
  propertyBrowserStatus = "Subscription disabled";
}

static void formatBacnetDeviceSummary(FixedTextBuffer& out) {
  if (!activeBacnetSession) {
    out.append("No device selected");
    return;
  }

  const BacnetIpEndpoint& endpoint = activeBacnetSession->endpoint();
  out.appendFormat("ID %lu @ %u.%u.%u.%u:%u",
                   static_cast<unsigned long>(
                     activeBacnetSession->deviceInstance()),
                   static_cast<unsigned>(endpoint.address[0]),
                   static_cast<unsigned>(endpoint.address[1]),
                   static_cast<unsigned>(endpoint.address[2]),
                   static_cast<unsigned>(endpoint.address[3]),
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
  out.append(bacnetReadStatusText(property.status, property.value));
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
  bacnetAppendObjectDisplayName(out, object.object);
  out.append(": ");
  const char* label =
    object.scanned != nullptr ? bacnetScannedLabelOrNull(*object.scanned)
                              : nullptr;
  out.appendShortened(label != nullptr && label[0] != '\0' ? label
                                                           : "<unnamed>");
  const char* description = object.scanned != nullptr
                              ? bacnetScannedDescriptionOrNull(*object.scanned)
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

static void formatPropertyBrowserObject(FixedTextBuffer& out) {
  if (!activeBacnetSession) {
    out.append("No device selected");
    return;
  }
  const BacnetObjectId object = propertyBrowser.objectId();
  if (!propertyBrowser.loaded()) {
    out.append(propertyBrowserStatus.c_str());
    return;
  }
  bacnetAppendObjectDisplayName(out, object);
  out.append(" | ");
  out.append(propertyBrowserStatus.c_str());
}

static void formatPropertyBrowserRows(FixedTextBuffer& out) {
  if (!propertyBrowser.loaded()) {
    out.append(propertyBrowserStatus.c_str());
    return;
  }
  const BacnetPropertySubscription* subscription = propertyBrowser.subscription();
  for (size_t i = 0; i < propertyBrowser.rowCount(); ++i) {
    const BacnetPropertyReadResult* row = propertyBrowser.row(i);
    if (row == nullptr) {
      continue;
    }
    if (!out.empty()) {
      out.append('\n');
    }
    out.appendFormat("%u. ", static_cast<unsigned>(i + 1));
    const char* propertyName = bacnetPropertyName(row->propertyId);
    if (std::strcmp(propertyName, "property") == 0) {
      out.appendFormat(
        "property-%lu: ", static_cast<unsigned long>(row->propertyId));
    } else {
      out.append(propertyName);
      out.append(": ");
    }
    const bool selected = propertyBrowser.hasSelection() &&
                          propertyBrowser.selectedIndex() == i;
    if (selected && subscription != nullptr && subscription->hasValue()) {
      out.append(subscription->lastValue().displayText());
      out.append(" [");
      out.append(bacnetReadStatusText(subscription->lastStatus()));
      out.append("]");
    } else if (row->status == BacnetPropertyReadStatus::Ack) {
      out.append(row->value.displayText());
      out.append(" [");
      out.append(bacnetValueTypeName(row->value.type));
      out.append("]");
    } else {
      out.append(bacnetPropertyReadStatusText(*row));
    }
  }
  if (out.empty()) {
    out.append("No properties collected");
  }
}

static void formatPropertyBrowserSubscription(FixedTextBuffer& out) {
  const BacnetPropertyReadResult* row = propertyBrowser.selectedRow();
  if (row == nullptr) {
    out.append("disabled");
    return;
  }
  out.append(bacnetPropertyName(row->propertyId));
  out.append(": ");
  out.append(propertyBrowser.subscriptionModeText());
  const BacnetPropertySubscription* subscription = propertyBrowser.subscription();
  if (subscription != nullptr && subscription->hasValue()) {
    out.append(" value=");
    out.append(subscription->lastValue().displayText());
  }
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
  bacnetAppendObjectDisplayName(out, bacnetStatusPreview.object);
  out.append(": ");
  const char* label = bacnetStatusPreview.labelSource != nullptr
                        ? bacnetScannedLabelOrNull(
                            *bacnetStatusPreview.labelSource)
                        : nullptr;
  if (label != nullptr && label[0] != '\0') {
    out.appendShortened(label);
  } else {
    bacnetAppendObjectDisplayName(out, bacnetStatusPreview.object);
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
  bacnetAppendStatusFlagsSummary(out, status);
  out.append(" event-state=");
  bacnetAppendEnumPropertySummary(
    out, status.eventStateStatus, bacnetEventStateText(status.eventState));
  out.append("\nreliability=");
  bacnetAppendEnumPropertySummary(
    out, status.reliabilityStatus, bacnetReliabilityText(status.reliability));
  out.append(" oos=");
  bacnetAppendBoolPropertySummary(
    out, status.outOfServiceStatus, status.outOfService);
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

static void fillPropertyBrowserRuntime(JsonObject& data) {
  char object[192] = {};
  FixedTextBuffer objectOut(object, sizeof(object));
  formatPropertyBrowserObject(objectOut);
  data["propertyBrowser_object"] = objectOut.data;

  char rows[(BacnetValue::kMaxTextLength + 80) *
            BacnetDemoPropertyBrowser::kMaxProperties] = {};
  FixedTextBuffer rowsOut(rows, sizeof(rows));
  formatPropertyBrowserRows(rowsOut);
  data["propertyBrowser_rows"] = rowsOut.data;

  char subscription[BacnetValue::kMaxTextLength + 96] = {};
  FixedTextBuffer subscriptionOut(subscription, sizeof(subscription));
  formatPropertyBrowserSubscription(subscriptionOut);
  data["propertyBrowser_subscription"] = subscriptionOut.data;
}

static void fillWatchedAnalogRuntime(JsonObject& data) {
  data["watchedAv_object"] = watchedAnalogValue.objectSummary();
  data["watchedAv_label"] = watchedAnalogValue.labelSummary();
  data["watchedAv_description"] = watchedAnalogValue.descriptionSummary();
  data["watchedAv_value"] = watchedAnalogValue.valueSummary();
#if CLIENT_DEMO_ENABLE_WATCH_METADATA_FIELD
  data["watchedAv_metadata"] = watchedAnalogValue.metadataSummary();
#endif
  data["watchedAv_alarm"] = watchedAnalogValue.alarmStateSummary();
  data["watchedAv_refresh"] = watchedAnalogValue.refreshSummary();
  data["polledAv_object"] = polledAnalogValue.objectSummary();
  data["polledAv_value"] = polledAnalogValue.valueSummary();
  data["polledAv_refresh"] = polledAnalogValue.refreshSummary();
  data["watchedAv_mode"] = watchedAnalogValue.updateModeSummary();
  data["polledAv_mode"] = polledAnalogValue.updateModeSummary();
  String overrideBvObject = "BV";
  overrideBvObject += String(overrideBinaryValueStatus.objectInstance());
  data["override_bv_object"] = overrideBvObject;
  String overrideBvState = overrideBinaryValueStatus.stateText();
  if (overrideBinaryValueStatus.state() == BacnetDemoBinaryValueState::Unknown) {
    overrideBvState += " (";
    overrideBvState += overrideBinaryValueStatus.detailText();
    overrideBvState += ")";
  }
  data["override_bv_state"] = overrideBvState;
#if defined(ESP_BACNET_ENABLE_WRITE_PROPERTY) && ESP_BACNET_ENABLE_WRITE_PROPERTY && \
  defined(ESP_BACNET_ENABLE_PRIORITY_WRITE) && ESP_BACNET_ENABLE_PRIORITY_WRITE
  data["override_status"] = bacnetOverrideStatus;
#else
  data["override_status"] = "WriteProperty/Priority feature unavailable";
#endif
}

static bool validOverrideConfiguration(uint8_t& priority) {
  const int configured = bacnetWritePriority.get();
  if (configured < 1 || configured > 16) {
    bacnetOverrideStatus = "Invalid Write Priority: use 1..16";
    return false;
  }
  priority = static_cast<uint8_t>(configured);
  return activeBacnetSession != nullptr;
}

static void setOverrideWriteStatus(const char* action,
                                   BacnetDeviceSessionWriteStatus status) {
  bacnetOverrideStatus = action;
  bacnetOverrideStatus += ": ";
  bacnetOverrideStatus += bacnetWriteStatusText(status);
}

static void refreshOverrideBvStatusAfterWrite(
  BacnetDeviceSessionWriteStatus writeStatus) {
  if (!activeBacnetSession) {
    return;
  }
  overrideBinaryValueStatus.readBackAfterWrite(
    *activeBacnetSession, writeStatus);
}

static void writeOverrideAv() {
#if defined(ESP_BACNET_ENABLE_WRITE_PROPERTY) && ESP_BACNET_ENABLE_WRITE_PROPERTY && \
  defined(ESP_BACNET_ENABLE_PRIORITY_WRITE) && ESP_BACNET_ENABLE_PRIORITY_WRITE
  uint8_t priority = 0;
  if (!validOverrideConfiguration(priority))
    return;
  if (!std::isfinite(bacnetOverrideAnalogInput)) {
    bacnetOverrideStatus = "Invalid AV value";
    return;
  }
  BacnetValue value;
  value.type = BacnetValueType::Real;
  value.realValue = bacnetOverrideAnalogInput;
  setOverrideWriteStatus("AV write", activeBacnetSession->object(BacnetObjectType::AnalogValue, static_cast<uint32_t>(bacnetOverrideAvInstance.get())).writePresentValue(value, priority, kBacnetScanReadTimeoutMs));
#else
  bacnetOverrideStatus = "WriteProperty/Priority feature unavailable";
#endif
}

static void relinquishOverrideAv() {
#if defined(ESP_BACNET_ENABLE_WRITE_PROPERTY) && ESP_BACNET_ENABLE_WRITE_PROPERTY && \
  defined(ESP_BACNET_ENABLE_PRIORITY_WRITE) && ESP_BACNET_ENABLE_PRIORITY_WRITE
  uint8_t priority = 0;
  if (!validOverrideConfiguration(priority))
    return;
  setOverrideWriteStatus("AV relinquish", activeBacnetSession->object(BacnetObjectType::AnalogValue, static_cast<uint32_t>(bacnetOverrideAvInstance.get())).relinquishPresentValue(priority, kBacnetScanReadTimeoutMs));
#else
  bacnetOverrideStatus = "WriteProperty/Priority feature unavailable";
#endif
}

static void writeOverrideBv(bool active) {
#if defined(ESP_BACNET_ENABLE_WRITE_PROPERTY) && ESP_BACNET_ENABLE_WRITE_PROPERTY && \
  defined(ESP_BACNET_ENABLE_PRIORITY_WRITE) && ESP_BACNET_ENABLE_PRIORITY_WRITE
  uint8_t priority = 0;
  if (!validOverrideConfiguration(priority))
    return;
  overrideBinaryValueStatus.configure(
    static_cast<uint32_t>(bacnetOverrideBvInstance.get()));
  BacnetValue value;
  value.type = BacnetValueType::Enumerated;
  value.unsignedValue = active ? 1U : 0U;
  const BacnetDeviceSessionWriteStatus writeStatus =
    activeBacnetSession->object(
                         BacnetObjectType::BinaryValue,
                         static_cast<uint32_t>(bacnetOverrideBvInstance.get()))
      .writePresentValue(value, priority, kBacnetScanReadTimeoutMs);
  setOverrideWriteStatus(active ? "BV set 1" : "BV set 0", writeStatus);
  refreshOverrideBvStatusAfterWrite(writeStatus);
#else
  bacnetOverrideStatus = "WriteProperty/Priority feature unavailable";
#endif
}

static void relinquishOverrideBv() {
#if defined(ESP_BACNET_ENABLE_WRITE_PROPERTY) && ESP_BACNET_ENABLE_WRITE_PROPERTY && \
  defined(ESP_BACNET_ENABLE_PRIORITY_WRITE) && ESP_BACNET_ENABLE_PRIORITY_WRITE
  uint8_t priority = 0;
  if (!validOverrideConfiguration(priority))
    return;
  overrideBinaryValueStatus.configure(
    static_cast<uint32_t>(bacnetOverrideBvInstance.get()));
  const BacnetDeviceSessionWriteStatus writeStatus =
    activeBacnetSession->object(
                         BacnetObjectType::BinaryValue,
                         static_cast<uint32_t>(bacnetOverrideBvInstance.get()))
      .relinquishPresentValue(priority, kBacnetScanReadTimeoutMs);
  setOverrideWriteStatus("BV relinquish", writeStatus);
  refreshOverrideBvStatusAfterWrite(writeStatus);
#else
  bacnetOverrideStatus = "WriteProperty/Priority feature unavailable";
#endif
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

  auto propertyBrowserGroup = ConfigManager.liveGroup("bacnetPropertyBrowser")
                                .page("Sensors", 10)
                                .card("BACnet Property Browser", 25)
                                .group("Bounded On-Demand Read", 1);
  ConfigManager.getRuntime().addRuntimeProvider(
    "bacnetPropertyBrowser", fillPropertyBrowserRuntime, 25);
  addRuntimeTextField("bacnetPropertyBrowser",
                      "propertyBrowser_object",
                      "Configured Object",
                      "Sensors",
                      "BACnet Property Browser",
                      "Bounded On-Demand Read",
                      10);
  propertyBrowserGroup
    .button("propertyBrowser_load", "Load Properties", []() {
      loadPropertyBrowser();
    })
    .order(20);
  addRuntimeTextField("bacnetPropertyBrowser",
                      "propertyBrowser_rows",
                      "Properties (max. 8)",
                      "Sensors",
                      "BACnet Property Browser",
                      "Bounded On-Demand Read",
                      30,
                      true);
  propertyBrowserGroup
    .button("propertyBrowser_subscribe", "Subscribe Selected Row", []() {
      subscribeSelectedPropertyBrowserProperty();
    })
    .order(40);
  propertyBrowserGroup
    .button("propertyBrowser_stop", "Stop Subscription", []() {
      stopPropertyBrowserSubscription();
    })
    .order(41);
  addRuntimeTextField("bacnetPropertyBrowser",
                      "propertyBrowser_subscription",
                      "Subscription Mode",
                      "Sensors",
                      "BACnet Property Browser",
                      "Bounded On-Demand Read",
                      50);

#if BACNET_DEMO_ENABLE_WATCHED_VALUES
  auto watchedAnalogGroup = ConfigManager.liveGroup("bacnetWatch")
                              .page("Sensors", 10)
                              .card("Watched Analog Value", 30)
                              .group("AV Watch", 1);

  ConfigManager.getRuntime().addRuntimeProvider(
    "bacnetWatch", fillWatchedAnalogRuntime, 30);
  addRuntimeTextField("bacnetWatch",
                      "watchedAv_object",
                      "AV200 Object",
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
                      "AV200 Present Value",
                      "Sensors",
                      "Watched Analog Value",
                      "AV Watch",
                      30);
#if CLIENT_DEMO_ENABLE_WATCH_METADATA_FIELD
  addRuntimeTextField("bacnetWatch",
                      "watchedAv_metadata",
                      "Metadata",
                      "Sensors",
                      "Watched Analog Value",
                      "AV Watch",
                      35);
#endif
  addRuntimeTextField("bacnetWatch",
                      "watchedAv_alarm",
                      "Alarm State",
                      "Sensors",
                      "Watched Analog Value",
                      "AV Watch",
                      39);
  addRuntimeTextField("bacnetWatch",
                      "watchedAv_refresh",
                      "AV200 Refresh",
                      "Sensors",
                      "Watched Analog Value",
                      "AV Watch",
                      45);
  addRuntimeTextField("bacnetWatch", "watchedAv_mode", "AV200 Mode", "Sensors", "Watched Analog Value", "AV Watch", 46);
  addRuntimeTextField("bacnetWatch", "polledAv_object", "AV201 Object", "Sensors", "Watched Analog Value", "AV Watch", 50);
  addRuntimeTextField("bacnetWatch", "polledAv_value", "AV201 Present Value", "Sensors", "Watched Analog Value", "AV Watch", 55);
  addRuntimeTextField("bacnetWatch", "polledAv_refresh", "AV201 Refresh", "Sensors", "Watched Analog Value", "AV Watch", 60);
  addRuntimeTextField("bacnetWatch", "polledAv_mode", "AV201 Mode", "Sensors", "Watched Analog Value", "AV Watch", 65);

  watchedAnalogGroup
    .button("watchedAv_details", "Log to Serial", []() { logWatchedAnalogDetails(); })
    .order(50);

  auto overrideGroup = ConfigManager.liveGroup("bacnetWatch")
                         .page("Sensors", 10)
                         .card("Manual Priority Overrides", 40)
                         .group("Explicit Actions", 1);
#if defined(ESP_BACNET_ENABLE_WRITE_PROPERTY) && ESP_BACNET_ENABLE_WRITE_PROPERTY && \
  defined(ESP_BACNET_ENABLE_PRIORITY_WRITE) && ESP_BACNET_ENABLE_PRIORITY_WRITE
  overrideGroup.floatInput("override_av_value", "AV Value", -1000000.0F, 1000000.0F, 0.0F, 3, []() { return bacnetOverrideAnalogInput; }, [](float value) { bacnetOverrideAnalogInput = value; }).order(10);
  overrideGroup.button("override_av_write", "Write AV", []() { writeOverrideAv(); }).order(20);
  overrideGroup.button("override_av_relinquish", "Relinquish AV", []() { relinquishOverrideAv(); }).order(21);
  overrideGroup.boolValue("override_bv_active", []() {
                 return overrideBinaryValueStatus.isActive();
               })
    .label("BV Active")
    .onLabel("active")
    .offLabel("inactive / unavailable")
    .order(25);
  addRuntimeTextField("bacnetWatch", "override_bv_object", "BV Object", "Sensors", "Manual Priority Overrides", "Explicit Actions", 26);
  addRuntimeTextField("bacnetWatch", "override_bv_state", "BV Present Value", "Sensors", "Manual Priority Overrides", "Explicit Actions", 27);
  overrideGroup.button("override_bv_0", "BV Set 0", []() { writeOverrideBv(false); }).order(30);
  overrideGroup.button("override_bv_1", "BV Set 1", []() { writeOverrideBv(true); }).order(31);
  overrideGroup.button("override_bv_relinquish", "BV Relinquish", []() { relinquishOverrideBv(); }).order(32);
  addRuntimeTextField("bacnetWatch", "override_status", "Last Result", "Sensors", "Manual Priority Overrides", "Explicit Actions", 40);
#else
  addRuntimeTextField("bacnetWatch", "override_status", "Write Feature", "Sensors", "Manual Priority Overrides", "Explicit Actions", 10);
#endif
#endif
}

static void sendWhoIs() {
  if (!bacnetStarted || !bacnetAddressConfigValid) {
    return;
  }

  if (!bacnetClient.sendWhoIs(bacnetIpEndpointFromArduino(whoIsDestination))) {
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
    Serial.println("[W] BACnet Who-Is send failed");
#endif
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
    demoLogging.log(BacnetDemoLogging::Level::Info,
                    "device property %s: %s",
                    property.name,
                    bacnetReadStatusText(property.status, property.value));
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
  if (notification.covPropertyCount > 0) {
    ++preview->covUpdateCount;
    preview->lastCovUpdateMs = millis();
  }
  for (size_t index = 0; index < notification.covPropertyCount; ++index) {
    const BacnetCovPropertyValue& property = notification.covProperties[index];
    if (property.property == BacnetPropertyId::StatusFlags) {
      preview->statusFlags = property.value;
      preview->statusFlagsStatus = BacnetPropertyReadStatus::Ack;
    }
  }
  char objectName[24] = {};
  FixedTextBuffer objectOut(objectName, sizeof(objectName));
  bacnetAppendObjectDisplayName(objectOut, notification.objectId);
  demoLogging.log(BacnetDemoLogging::Level::Info,
                  "subscription update %s present-value %s=%s",
                  objectOut.c_str(),
                  bacnetSubscriptionReasonText(notification),
                  valueObjectPresentValueText(*preview) != nullptr
                    ? valueObjectPresentValueText(*preview)
                    : bacnetReadStatusText(preview->presentValueStatus));
}

static void onStatusFlagsUpdate(
  const BacnetSubscriptionNotification& notification) {
  auto* preview =
    static_cast<BacnetValueObjectPreview*>(notification.userData);
  if (preview == nullptr) {
    return;
  }
  preview->statusFlagsStatus = notification.status == BacnetDeviceSessionReadStatus::Ack
                                 ? BacnetPropertyReadStatus::Ack
                                 : BacnetPropertyReadStatus::Error;
  if (notification.hasValue && notification.value != nullptr) {
    preview->statusFlags = *notification.value;
  }
}

static bool subscribePresentValue(BacnetDeviceSession& session,
                                  BacnetValueObjectPreview& preview) {
  if (!preview.discovered) {
    return false;
  }

  BacnetSubscribeOptions options;
  options.preferCov = BACNET_DEMO_ENABLE_OBJECT_COV != 0;
  options.covLifetimeSeconds = processObjectCovLifetimeSeconds();
  options.covRenewBeforeSeconds = 5;
  options.usePropertyCov = options.preferCov &&
                           bacnetIsAnalogProcessObject(preview.object.type);
  options.hasCovIncrement = options.usePropertyCov;
  options.covIncrement = options.usePropertyCov ? 0.5F : 0.0F;
  options.fallbackPollMs = kBacnetSubscriptionFallbackPollMs;
  options.timeoutMs = kBacnetScanReadTimeoutMs;
  options.immediateFirstRead = false;
  options.notifyOnStatusChange = true;

  preview.subscription.reset(new BacnetPropertySubscription(
    session.object(preview.object)
      .property(BacnetPropertyId::PresentValue)
      .subscribe(onPresentValueUpdate, &preview, options)));
  preview.usesPropertyCov = options.usePropertyCov;
#if BACNET_DEMO_ENABLE_OBJECT_COV
  BacnetSubscribeOptions statusOptions;
  statusOptions.fallbackPollMs = 300000;
  statusOptions.timeoutMs = kBacnetScanReadTimeoutMs;
  statusOptions.immediateFirstRead = true;
  statusOptions.notifyOnStatusChange = true;
  preview.statusSubscription.reset(new BacnetPropertySubscription(
    session.object(preview.object)
      .property(BacnetPropertyId::StatusFlags)
      .subscribe(onStatusFlagsUpdate, &preview, statusOptions)));
#endif
  demoLogging.log(BacnetDemoLogging::Level::Info,
                  "subscription created %s present-value mode=%s lifetime=%lu fallbackMs=%lu active=%s",
                  bacnetObjectDisplayName(preview.object).c_str(),
                  options.usePropertyCov ? "property-cov" :
                    (options.preferCov ? "object-cov" : "polling"),
                  static_cast<unsigned long>(options.covLifetimeSeconds),
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

  bacnetScanStoredObjects = analogStored + binaryStored + multiStateStored;
  bacnetScanStatus = bacnetScanTerminalStatus(scan);

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
#if BACNET_DEMO_ENABLE_WATCHED_VALUES
  watchedAnalogValue.setup(session);
  polledAnalogValue.setup(session);
#endif

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
  if (scan.stored == 0 ||
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
#if BACNET_DEMO_ENABLE_WATCHED_VALUES
      watchedAnalogValue.setup(*activeBacnetSession);
      polledAnalogValue.setup(*activeBacnetSession);
#endif
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
    BacnetDeviceSession::fromEndpoint(
      bacnetClient, deviceInstance, bacnetIpEndpointFromArduino(address, port))));
  bacnetDeviceSelected = true;
  bacnetScanStatus = "Scan queued for selected device";

#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
  Serial.print("[I] BACnet selected device ");
  Serial.print(deviceInstance);
  Serial.print(" at ");
  Serial.print(address);
  Serial.print(":");
  Serial.println(port);
#endif
  demoLogging.log(BacnetDemoLogging::Level::Info, "selected device %lu at %s:%u", static_cast<unsigned long>(deviceInstance), address.toString().c_str(), static_cast<unsigned>(port));

  bacnetScanRequested = true;
  demoLogging.log(BacnetDemoLogging::Level::Info, "scan queued for selected device %lu", static_cast<unsigned long>(deviceInstance));
}

static void selectBacnetDevice(const BacnetIAmDevice& device) {
  if (bacnetDeviceSelected || device.endpoint.isZero()) {
    return;
  }

  resetBacnetPreviews();
  activeBacnetVendorId = device.vendorId;
  activeBacnetSession.reset(new BacnetDeviceSession(
    BacnetDeviceSession::fromIAm(bacnetClient, device)));
  bacnetDeviceSelected = true;
  bacnetScanStatus = "Scan queued for selected device";

#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
  Serial.print("[I] BACnet selected device ");
  Serial.print(device.deviceInstance);
  Serial.print(" at ");
  Serial.print(bacnetIpAddressFromEndpoint(device.endpoint));
  Serial.print(":");
  Serial.println(activeBacnetSession->port());
#endif
  const IPAddress address = bacnetIpAddressFromEndpoint(device.endpoint);
  demoLogging.log(BacnetDemoLogging::Level::Info, "selected device %lu at %s:%u", static_cast<unsigned long>(device.deviceInstance), address.toString().c_str(), static_cast<unsigned>(device.endpoint.port));

  bacnetScanRequested = true;
  demoLogging.log(BacnetDemoLogging::Level::Info, "scan queued for selected device %lu", static_cast<unsigned long>(device.deviceInstance));
}

static void clearBacnetRuntime() {
  if (activeBacnetSession && scanJob.isActive()) {
    activeBacnetSession->cancelObjectListScan(scanJob);
  }
  if (activeBacnetSession) {
    propertyBrowser.reset(activeBacnetSession.get());
  }
  resetValueObjects(analogValues, kBacnetMaxFoundObjectsToDisplay);
  resetValueObjects(binaryValues, kBacnetMaxFoundObjectsToDisplay);
  resetValueObjects(multiStateValues, kBacnetMaxFoundObjectsToDisplay);
  bacnetStatusPreview = BacnetObjectStatusPreview{};
  watchedAnalogValue.reset("not read");
  polledAnalogValue.reset("not read");
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
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
      Serial.println("[E] BACnet disabled: invalid BACnet IP config");
#endif
      demoLogging.log(BacnetDemoLogging::Level::Error, "disabled: invalid BACnet IP config");
    }
    return;
  }

  if (!bacnetClient.begin()) {
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
    Serial.println("[E] BACnet UDP listener failed to start");
#endif
    demoLogging.log(BacnetDemoLogging::Level::Error, "client UDP listener failed");
    return;
  }

  bacnetStarted = true;
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
  Serial.print("[I] BACnet client started on UDP port ");
  Serial.println(bacnetClient.localPort());
#endif

  sendWhoIs();
  selectBacnetDevice(BACNET_TARGET_DEVICE_INSTANCE,
                     configuredBacnetTargetAddress,
                     BACNET_TARGET_PORT,
                     0);
}

static void pollBacnetDiscovery() {
  if (!bacnetStarted || bacnetDeviceSelected) {
    return;
  }

  BacnetIAmDevice device;
  if (bacnetClient.pollIAm(device)) {
    selectBacnetDevice(device);
  }

  if (!bacnetDeviceSelected && millis() - lastWhoIsAt >= kWhoIsIntervalMs) {
    sendWhoIs();
  }
}

static void pollBacnetSubscriptions() {
  // The initial Object-List scan owns the session.  Starting a COV request or
  // fallback poll before it has finished would make the scan reject itself as
  // busy and leave the bounded browser without discovered objects.
  if (!activeBacnetSession || bacnetScanRunning || !bacnetScanFinished) {
    return;
  }

  const uint32_t now = millis();
  if (propertyBrowser.loading()) {
    propertyBrowser.poll(*activeBacnetSession, now);
    updatePropertyBrowserStatus();
    return;
  }

  for (size_t i = 0; i < kBacnetMaxFoundObjectsToDisplay; ++i) {
    if (analogValues[i].subscription) {
      activeBacnetSession->poll(*analogValues[i].subscription, now);
    }
    if (analogValues[i].statusSubscription) {
      activeBacnetSession->poll(*analogValues[i].statusSubscription, now);
    }
    if (binaryValues[i].subscription) {
      activeBacnetSession->poll(*binaryValues[i].subscription, now);
    }
    if (binaryValues[i].statusSubscription) {
      activeBacnetSession->poll(*binaryValues[i].statusSubscription, now);
    }
    if (multiStateValues[i].subscription) {
      activeBacnetSession->poll(*multiStateValues[i].subscription, now);
    }
    if (multiStateValues[i].statusSubscription) {
      activeBacnetSession->poll(*multiStateValues[i].statusSubscription, now);
    }
  }

#if BACNET_DEMO_ENABLE_WATCHED_VALUES
  watchedAnalogValue.poll(*activeBacnetSession, now);
  polledAnalogValue.poll(*activeBacnetSession, now);
#endif
  propertyBrowser.poll(*activeBacnetSession, now);
  updatePropertyBrowserStatus();
#if BACNET_DEMO_ENABLE_WATCHED_VALUES
  overrideBinaryValueStatus.configure(
    static_cast<uint32_t>(bacnetOverrideBvInstance.get()));
  overrideBinaryValueStatus.poll(*activeBacnetSession, now);
#endif
}

static void registerBacnetOverrideSettings() {
#if BACNET_DEMO_ENABLE_WATCHED_VALUES
#if defined(ESP_BACNET_ENABLE_WRITE_PROPERTY) && ESP_BACNET_ENABLE_WRITE_PROPERTY && \
  defined(ESP_BACNET_ENABLE_PRIORITY_WRITE) && ESP_BACNET_ENABLE_PRIORITY_WRITE
  ConfigManager.addSetting(&bacnetWritePriority);
  ConfigManager.addSetting(&bacnetOverrideAvInstance);
  ConfigManager.addSetting(&bacnetOverrideBvInstance);
#endif
  ConfigManager.addSetting(&bacnetPollingAvInstance);
#endif
  ConfigManager.addSetting(&bacnetCovLifetime);
  ConfigManager.addSetting(&bacnetBrowserObjectType);
  ConfigManager.addSetting(&bacnetBrowserObjectInstance);
  ConfigManager.addSetting(&bacnetBrowserPropertyRow);
}

#if BACNET_DEMO_USE_ETHERNET
static void registerEthernetSettings() {
  ConfigManager.setCategoryLayoutOverride(
    "Ethernet", "Network", "Network", "Ethernet Settings", 10);
  ConfigManager.addSettingsPage("Network", 10);
  ConfigManager.addSettingsGroup(
    "Network", "Network", "Ethernet Settings", 10);
  ConfigManager.addSetting(&ethernetIp);
  ConfigManager.addSetting(&ethernetSubnet);
  ConfigManager.addSetting(&ethernetGateway);
  ConfigManager.addSetting(&ethernetDns);
  ConfigManager.addSetting(&settingsPassword);
}

static void startEthernetServices() {
  if (!ethernetServicesStarted) {
    ConfigManager.startWebServerOnNetwork();
#if CM_ENABLE_OTA
    ConfigManager.setupOTA(APP_NAME, systemSettings.otaPassword.get());
#endif
    configTzTime(ntpSettings.tz.get().c_str(),
                 ntpSettings.server1.get().c_str(),
                 ntpSettings.server2.get().c_str());
    ethernetServicesStarted = true;
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
    Serial.println("[I] ConfigManager services started on Ethernet");
#endif
  }
  startBacnetClient();
}

static void updateEthernetNetwork() {
  const bool connected = bacnet_example::EthernetNetwork::hasIp();
  if (connected && !ethernetWasConnected) {
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
    Serial.print("[I] Ethernet station IP: ");
    Serial.println(bacnet_example::EthernetNetwork::localIp());
#endif
    startEthernetServices();
  } else if (!connected && ethernetWasConnected) {
    clearBacnetRuntime();
    bacnetClient.end();
    bacnetStarted = false;
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
    Serial.println("[W] Ethernet network unavailable");
#endif
  }
  ethernetWasConnected = connected;
}
#endif

void setup() {
  Serial.begin(115200);
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
  Serial.println("[I] BACnet client demo starting");
#endif

  ConfigManagerClass::setLogger([](const char* msg) {
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
    Serial.print("[D] CM ");
    Serial.println(msg);
#else
    (void)msg;
#endif
  });

  resetBacnetPreviews();
#if BACNET_DEMO_ENABLE_WATCHED_VALUES
  watchedAnalogValue.setLogger(demoLogging.watchedAnalogCallback());
  polledAnalogValue.setLogger(demoLogging.watchedAnalogCallback());
#endif
  ConfigManager.setAppName(APP_NAME);
  ConfigManager.setAppTitle(APP_NAME);
  ConfigManager.setVersion(APP_VERSION);
  ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);
  ConfigManager.enableBuiltinSystemProvider();
  ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE,
                             sizeof(GLOBAL_THEME_OVERRIDE) - 1);
  demoLogging.setup();
  configureBacnetAddresses();

  registerBacnetOverrideSettings();

#if BACNET_DEMO_USE_ETHERNET
  registerEthernetSettings();
#else
  coreSettings.attachWiFi(ConfigManager);
#endif
  coreSettings.attachSystem(ConfigManager);
  coreSettings.attachNtp(ConfigManager);

  ConfigManager.loadAll();
#if BACNET_DEMO_ENABLE_WATCHED_VALUES
  watchedAnalogValue.configure(
    static_cast<uint32_t>(bacnetOverrideAvInstance.get()), true, static_cast<uint32_t>(bacnetCovLifetime.get()));
  polledAnalogValue.configure(
    static_cast<uint32_t>(bacnetPollingAvInstance.get()), false, 0);
  overrideBinaryValueStatus.configure(
    static_cast<uint32_t>(bacnetOverrideBvInstance.get()));
#endif
  setupNetworkDefaults();

#if BACNET_DEMO_ENABLE_RUNTIME_GUI
  setupRuntimeUI();
#endif

#if !BACNET_DEMO_USE_ETHERNET && defined(WIFI_FILTER_MAC_PRIORITY)
  ConfigManager.setAccessPointMacPriority(WIFI_FILTER_MAC_PRIORITY);
#endif

#if BACNET_DEMO_USE_ETHERNET
  const bacnet_example::EthernetConfig ethernetConfig{
    MY_USE_DHCP,
    ethernetIp.get().c_str(),
    ethernetGateway.get().c_str(),
    ethernetSubnet.get().c_str(),
    ethernetDns.get().c_str(),
  };
  if (!bacnet_example::EthernetNetwork::begin(APP_NAME, ethernetConfig)) {
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
    Serial.println("[E] Ethernet startup failed");
#endif
  }
#else
  ConfigManager.startWebServer();
#endif

#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
  Serial.println("[I] Setup completed, starting main loop");
#endif
}

#if !BACNET_DEMO_USE_ETHERNET
void onWiFiConnected() {
  wifiServices.onConnected(ConfigManager, APP_NAME, systemSettings, ntpSettings);
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
  Serial.print("[I] WiFi station IP: ");
  Serial.println(WiFi.localIP());
#endif
  startBacnetClient();
}

void onWiFiDisconnected() {
  wifiServices.onDisconnected();
  clearBacnetRuntime();
  bacnetClient.end();
  bacnetStarted = false;
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
  Serial.println("[W] WiFi disconnected");
#endif
}

void onWiFiAPMode() {
  wifiServices.onAPMode();
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
  Serial.print("[I] WiFi AP mode IP: ");
  Serial.println(WiFi.softAPIP());
#endif
}
#endif

static void setupNetworkDefaults() {
#if BACNET_DEMO_USE_ETHERNET
  if (ethernetIp.get().isEmpty()) {
    ethernetIp.set(MY_ETHERNET_IP);
    ethernetSubnet.set(MY_SUBNET_MASK);
    ethernetGateway.set(MY_GATEWAY_IP);
    ethernetDns.set(MY_DNS_IP);
    settingsPassword.set(SETTINGS_PASSWORD);
#if CM_ENABLE_OTA
    systemSettings.otaPassword.set(OTA_PASSWORD);
#endif
    ConfigManager.saveAll();
  }

  ConfigManager.setSettingsPassword(settingsPassword.get());
  settingsPassword.setCallback([](const String& password) {
    ConfigManager.setSettingsPassword(password);
  });
#else
  if (wifiSettings.wifiSsid.get().isEmpty()) {
#if BACNET_DEMO_HAS_SECRETS
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
    Serial.println("[I] WiFi SSID empty, applying local secret defaults");
#endif
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
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
    Serial.println("[I] Restarting after applying WiFi defaults");
#endif
    delay(500);
    ESP.restart();
#else
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
    Serial.println("[W] WiFi SSID empty and secret/secrets.h missing");
#endif
#endif
  }

  if (systemSettings.otaPassword.get() != OTA_PASSWORD) {
    systemSettings.otaPassword.save(OTA_PASSWORD);
  }
#endif
}

void loop() {
#if BACNET_DEMO_USE_ETHERNET
  updateEthernetNetwork();
#else
  ConfigManager.getWiFiManager().update();
#endif
  ConfigManager.handleClient();
  demoLogging.loop();
  bacnetClient.logger().tick();
  pollBacnetDiscovery();
  processPendingBacnetRescan();
  pollBacnetSubscriptions();
  scanSelectedBacnetDevice();

  static unsigned long lastLoopLog = 0;
#if BACNET_DEMO_ENABLE_SERIAL_DIAGNOSTICS
  if (millis() - lastLoopLog > 60000) {
    lastLoopLog = millis();
#if BACNET_DEMO_USE_ETHERNET
    Serial.print("[D] Loop running, Ethernet IP=");
    Serial.print(bacnet_example::EthernetNetwork::localIp());
#else
    Serial.print("[D] Loop running, WiFi status=");
    Serial.print(WiFi.status());
#endif
    Serial.print(" heap=");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" minHeap=");
    Serial.print(ESP.getMinFreeHeap());
    Serial.print(" maxAlloc=");
    Serial.println(ESP.getMaxAllocHeap());
  }
#endif
}
