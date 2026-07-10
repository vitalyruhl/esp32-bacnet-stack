// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <Arduino.h>
#include <EspBacnet.h>

#ifndef EXAMPLE_USE_ETHERNET
#define EXAMPLE_USE_ETHERNET 0
#endif

#if EXAMPLE_USE_ETHERNET
#include <ETH.h>
#include <ExampleEthernet.h>

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#else
#include "secret/secrets.example.h"
#endif
#endif

#if 0
#include <BME280_I2C.h>
#include <Ticker.h>

#include <math.h>

#ifndef BME280_ADDRESS
#define BME280_ADDRESS 0x76
#endif

#define I2C_SDA 21
#define I2C_SCL 22

static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
.live-cards {
  align-items: flex-start !important;
  grid-template-columns: minmax(620px, 820px) minmax(280px, 1fr) !important;
})CSS";

static BME280_I2C bme280;
static Ticker environmentalTicker;
static float bacnetTemperatureC = 0.0f;
static float bacnetHumidityPct = 0.0f;
static float bacnetDewPointC = 0.0f;
static float bacnetPressureHpa = 0.0f;

static float computeDewPointC(float temperatureC, float humidityPct) {
  static constexpr float kA = 17.62f;
  static constexpr float kB = 243.12f;
  const float safeHumidity = constrain(humidityPct, 1.0f, 100.0f);
  const float gamma =
    log(safeHumidity / 100.0f) + (kA * temperatureC) / (kB + temperatureC);
  return (kB * gamma) / (kA - gamma);
}

static void readBme280ForBacnet() {
  bme280.read();
  bacnetTemperatureC = bme280.data.temperature;
  bacnetHumidityPct = bme280.data.humidity;
  bacnetPressureHpa = bme280.data.pressure;
  bacnetDewPointC = computeDewPointC(bacnetTemperatureC, bacnetHumidityPct);
}

static void setupBme280ForBacnet() {
  Serial.println("[I] Initializing BME280 sensor");
  bme280.setAddress(BME280_ADDRESS, I2C_SDA, I2C_SCL);
  const bool ok = bme280.begin(
    bme280.BME280_STANDBY_0_5,
    bme280.BME280_FILTER_OFF,
    bme280.BME280_SPI3_DISABLE,
    bme280.BME280_OVERSAMPLING_1,
    bme280.BME280_OVERSAMPLING_1,
    bme280.BME280_OVERSAMPLING_1,
    bme280.BME280_MODE_NORMAL);
  if (!ok) {
    Serial.println("[W] BME280 not initialized, continuing without sensor");
    return;
  }

  Serial.println("[I] BME280 ready");
  environmentalTicker.attach(30.0f, readBme280ForBacnet);
  readBme280ForBacnet();
}
#endif

BacnetServer bacnetServer;

void setup() {
  Serial.begin(115200);
#if EXAMPLE_USE_ETHERNET
  const bacnet_example::EthernetConfig ethernetConfig{
    MY_USE_DHCP,
    MY_ETHERNET_IP,
    MY_GATEWAY_IP,
    MY_SUBNET_MASK,
    MY_DNS_IP,
  };
  if (!bacnet_example::EthernetNetwork::begin(
        "bacnet-server-demo", ethernetConfig) ||
      !bacnet_example::EthernetNetwork::waitForIp(20000)) {
    Serial.println("[E] BACnet server network startup failed");
    return;
  }
#endif
  bacnetServer.begin(1234);
#if 0
  setupBme280ForBacnet();
#endif
  Serial.println("[I] BACnet server demo started");
#if EXAMPLE_USE_ETHERNET
  Serial.print("[I] BACnet server Ethernet IP: ");
  Serial.println(bacnet_example::EthernetNetwork::localIp());
#endif
}

void loop() {
  delay(1000);
}
