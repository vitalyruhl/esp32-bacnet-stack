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
static constexpr size_t kMaxBacnetDevices = 5;
static constexpr size_t kMaxBacnetProperties = 5;
static constexpr size_t kMaxBacnetMvObjects = 5;
static constexpr size_t kBacnetPreviewPropertyCount = 4;
static constexpr uint8_t kReadPropertyInvokeId = 1;
static constexpr uint16_t kBacnetDeviceObjectType = 8;
static constexpr uint16_t kBacnetMultiStateValueObjectType = 19;

// Temporary WAGO hardware-validation list until objectList parsing is added.
static constexpr uint32_t kTemporaryWagoMvInstances[kMaxBacnetMvObjects] = {
    0, 1, 2, 3, 4};

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

struct BacnetMvPresentValuePreview {
  bool configured = false;
  BacnetObjectId object;
  String value = "";
  String status = "not read";
  bool requested = false;
  bool received = false;
  bool failed = false;
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
  BacnetMvPresentValuePreview mvPresentValues[kMaxBacnetMvObjects];
};

enum class BacnetReadTarget {
  None,
  DeviceProperty,
  MvPresentValue,
};

static BacnetDevicePreview bacnetDevices[kMaxBacnetDevices];
static BacnetReadTarget activeReadTarget = BacnetReadTarget::None;
static int activeReadDeviceIndex = -1;
static int activeReadPropertyIndex = -1;
static int activeReadMvIndex = -1;
static uint8_t activeReadInvokeId = 0;
static unsigned long activeReadStartedAt = 0;

static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
.myCSSTempClass { color:rgb(198, 16, 16) !important; font-weight:900!important; font-size: 1.2rem!important; }
)CSS";

void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();
static void setupNetworkDefaults();

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

static void initializeTemporaryWagoMvObjects(BacnetDevicePreview& device) {
  if (device.deviceId != kWagoDeviceInstance ||
      device.mvPresentValues[0].configured) {
    return;
  }

  for (size_t i = 0; i < kMaxBacnetMvObjects; ++i) {
    BacnetMvPresentValuePreview& mv = device.mvPresentValues[i];
    mv.configured = true;
    mv.object = BacnetObjectId{kBacnetMultiStateValueObjectType,
                               kTemporaryWagoMvInstances[i]};
    mv.status = "pending";
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

static String bacnetMvPresentValueSummary(size_t deviceIndex, size_t mvIndex) {
  if (deviceIndex >= kMaxBacnetDevices || mvIndex >= kMaxBacnetMvObjects ||
      !bacnetDevices[deviceIndex].active ||
      !bacnetDevices[deviceIndex].mvPresentValues[mvIndex].configured) {
    return "not configured";
  }

  const BacnetMvPresentValuePreview& mv =
      bacnetDevices[deviceIndex].mvPresentValues[mvIndex];
  if (mv.received) {
    return mv.value;
  }
  return mv.status;
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

  for (size_t deviceIndex = 0; deviceIndex < kMaxBacnetDevices;
       ++deviceIndex) {
    const String groupName = String("Device ") + String(deviceIndex + 1);
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

    for (size_t mvIndex = 0; mvIndex < kMaxBacnetMvObjects; ++mvIndex) {
      const String mvKey = String("device") + String(deviceIndex) + "_mv" +
                           String(mvIndex) + "_presentValue";
      const String mvLabel = String("MV ") + String(mvIndex) + " presentValue";
      bacnetDeviceGroup
          .value(mvKey.c_str(), [deviceIndex, mvIndex]() {
            return bacnetMvPresentValueSummary(deviceIndex, mvIndex);
          })
          .label(mvLabel.c_str())
          .order(40 + mvIndex);
    }
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
  } else {
    Serial.println("[W] BACnet Who-Is send failed");
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
  initializeTemporaryWagoMvObjects(device);

  if (isNewDevice) {
    for (size_t i = 0; i < kBacnetPreviewPropertyCount; ++i) {
      device.properties[i].status = "pending";
    }
  }

  return slot;
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
      activeReadTarget = BacnetReadTarget::DeviceProperty;
      activeReadPropertyIndex = static_cast<int>(propertyIndex);
      activeReadMvIndex = -1;
      activeReadInvokeId = kReadPropertyInvokeId;
      activeReadStartedAt = millis();
      property.requested = true;
      property.status = "reading";

      Serial.print("[I] Sending BACnet ReadProperty ");
      Serial.print(property.name);
      Serial.print(" to ");
      Serial.print(device.address);
      Serial.print(":");
      Serial.println(BacnetClient::kDefaultPort);

      if (bacnetClient.sendReadProperty(
              device.address,
              BacnetObjectId{kBacnetDeviceObjectType, device.deviceId},
              property.id, activeReadInvokeId)) {
        Serial.println("[I] BACnet ReadProperty sent");
      } else {
        Serial.println("[W] BACnet ReadProperty send failed");
        property.failed = true;
        property.status = "send failed";
        activeReadTarget = BacnetReadTarget::None;
        activeReadDeviceIndex = -1;
        activeReadPropertyIndex = -1;
        requestNextBacnetProperty();
      }
      return;
    }
  }

  for (size_t deviceIndex = 0; deviceIndex < kMaxBacnetDevices; ++deviceIndex) {
    BacnetDevicePreview& device = bacnetDevices[deviceIndex];
    if (!device.active || device.address == IPAddress(0, 0, 0, 0)) {
      continue;
    }

    for (size_t mvIndex = 0; mvIndex < kMaxBacnetMvObjects; ++mvIndex) {
      BacnetMvPresentValuePreview& mv = device.mvPresentValues[mvIndex];
      if (!mv.configured || mv.requested || mv.received || mv.failed) {
        continue;
      }

      activeReadDeviceIndex = static_cast<int>(deviceIndex);
      activeReadTarget = BacnetReadTarget::MvPresentValue;
      activeReadPropertyIndex = -1;
      activeReadMvIndex = static_cast<int>(mvIndex);
      activeReadInvokeId = kReadPropertyInvokeId;
      activeReadStartedAt = millis();
      mv.requested = true;
      mv.status = "reading";

      Serial.print("[I] Sending BACnet ReadProperty presentValue to ");
      Serial.print("multi-state-value,");
      Serial.print(mv.object.instance);
      Serial.print(" at ");
      Serial.print(device.address);
      Serial.print(":");
      Serial.println(BacnetClient::kDefaultPort);

      if (bacnetClient.sendReadProperty(device.address, mv.object,
                                        BacnetPropertyId::PresentValue,
                                        activeReadInvokeId)) {
        Serial.println("[I] BACnet ReadProperty sent");
      } else {
        Serial.println("[W] BACnet ReadProperty send failed");
        mv.failed = true;
        mv.status = "send failed";
        activeReadTarget = BacnetReadTarget::None;
        activeReadDeviceIndex = -1;
        activeReadMvIndex = -1;
        requestNextBacnetProperty();
      }
      return;
    }
  }
}

static void pollReadProperty() {
  if (activeReadTarget == BacnetReadTarget::None ||
      activeReadDeviceIndex < 0) {
    return;
  }

  BacnetDevicePreview& device = bacnetDevices[activeReadDeviceIndex];
  BacnetPropertyId expectedProperty = BacnetPropertyId::ObjectName;
  if (activeReadTarget == BacnetReadTarget::DeviceProperty &&
      activeReadPropertyIndex >= 0) {
    expectedProperty = device.properties[activeReadPropertyIndex].id;
  } else if (activeReadTarget == BacnetReadTarget::MvPresentValue &&
             activeReadMvIndex >= 0) {
    expectedProperty = BacnetPropertyId::PresentValue;
  } else {
    activeReadTarget = BacnetReadTarget::None;
    activeReadDeviceIndex = -1;
    activeReadPropertyIndex = -1;
    activeReadMvIndex = -1;
    return;
  }

  BacnetValue value;
  if (bacnetClient.pollReadProperty(value, activeReadInvokeId,
                                    expectedProperty)) {
    if (activeReadTarget == BacnetReadTarget::DeviceProperty) {
      BacnetPropertyPreview& property =
          device.properties[activeReadPropertyIndex];
      property.value = value.text;
      property.status = "ok";
      property.received = true;

      Serial.print("[I] BACnet ReadProperty ");
      Serial.print(property.name);
      Serial.print("=");
      Serial.println(property.value);
    } else {
      BacnetMvPresentValuePreview& mv =
          device.mvPresentValues[activeReadMvIndex];
      mv.value = value.text;
      mv.status = "ok";
      mv.received = true;

      Serial.print("[I] BACnet ReadProperty multi-state-value,");
      Serial.print(mv.object.instance);
      Serial.print(" presentValue=");
      Serial.println(mv.value);
    }

    activeReadTarget = BacnetReadTarget::None;
    activeReadDeviceIndex = -1;
    activeReadPropertyIndex = -1;
    activeReadMvIndex = -1;
    requestNextBacnetProperty();
    return;
  }

  if (millis() - activeReadStartedAt < kReadPropertyTimeoutMs) {
    return;
  }

  if (activeReadTarget == BacnetReadTarget::DeviceProperty) {
    BacnetPropertyPreview& property =
        device.properties[activeReadPropertyIndex];
    property.failed = true;
    property.status = "timeout";
    Serial.print("[W] BACnet ReadProperty timeout ");
    Serial.println(property.name);
  } else {
    BacnetMvPresentValuePreview& mv =
        device.mvPresentValues[activeReadMvIndex];
    mv.failed = true;
    mv.status = "timeout";
    Serial.print("[W] BACnet ReadProperty timeout multi-state-value,");
    Serial.print(mv.object.instance);
    Serial.println(" presentValue");
  }

  activeReadTarget = BacnetReadTarget::None;
  activeReadDeviceIndex = -1;
  activeReadPropertyIndex = -1;
  activeReadMvIndex = -1;
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
