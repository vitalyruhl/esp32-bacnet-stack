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
static constexpr uint32_t kWhoIsIntervalMs = 30000;
static constexpr uint8_t kReadPropertyInvokeId = 1;

static float temperature = 0.0f;
static float dewPoint = 0.0f;
static float humidity = 0.0f;
static float pressure = 0.0f;
static bool bacnetStarted = false;
static bool receivedIAmSinceLastWhoIs = true;
static bool readPropertyRequested = false;
static bool readPropertyReceived = false;
static uint32_t whoIsCount = 0;
static unsigned long lastWhoIsAt = 0;

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

static void sendReadProperty(BacnetIAmDevice device) {
  if (readPropertyRequested || device.deviceInstance != 9001) {
    return;
  }

  Serial.print("[I] Sending BACnet ReadProperty objectName to ");
  Serial.print(kWagoAddress);
  Serial.print(":");
  Serial.println(BacnetClient::kDefaultPort);

  if (bacnetClient.sendReadProperty(kWagoAddress, BacnetObjectId{8, 9001},
                                    BacnetPropertyId::ObjectName,
                                    kReadPropertyInvokeId)) {
    readPropertyRequested = true;
    Serial.println("[I] BACnet ReadProperty sent");
  } else {
    Serial.println("[W] BACnet ReadProperty send failed");
  }
}

static void pollReadProperty() {
  if (!readPropertyRequested || readPropertyReceived) {
    return;
  }

  BacnetValue value;
  if (!bacnetClient.pollReadProperty(value, kReadPropertyInvokeId,
                                     BacnetPropertyId::ObjectName)) {
    return;
  }

  readPropertyReceived = true;
  Serial.print("[I] BACnet ReadProperty objectName=");
  Serial.println(value.text);
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
    sendReadProperty(device);
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
