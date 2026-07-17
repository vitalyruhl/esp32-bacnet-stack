// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <Arduino.h>
#include <WiFi.h>

namespace bacnet_example {

struct WiFiNetworkConfig {
  bool useDhcp = true;
  const char* ssid = nullptr;
  const char* password = nullptr;
  const char* localIp = nullptr;
  const char* gateway = nullptr;
  const char* subnet = nullptr;
  const char* dns = nullptr;
  uint32_t connectTimeoutMs = 30000;
  uint32_t retryDelayMs = 250;
  bool printProgress = false;
};

// ESP32-only example support. It deliberately owns no BACnet transport,
// session, request, or write state.
class Esp32WiFiNetwork {
public:
  static bool begin(const WiFiNetworkConfig& config) {
    if (config.ssid == nullptr || config.ssid[0] == '\0') {
      Serial.println("[E] WiFi SSID is not configured");
      return false;
    }
    WiFi.mode(WIFI_STA);
    if (!config.useDhcp && !configureStaticIp(config)) {
      return false;
    }

    Serial.println("[I] Connecting WiFi");
    WiFi.begin(config.ssid, config.password);
    const uint32_t startedAt = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startedAt < config.connectTimeoutMs) {
      delay(config.retryDelayMs);
      if (config.printProgress) {
        Serial.print('.');
      }
      yield();
    }
    if (config.printProgress) {
      Serial.println();
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[E] WiFi connection timed out");
      return false;
    }
    Serial.print("[I] local WiFi IP ");
    Serial.println(WiFi.localIP());
    return true;
  }

  static bool connected() {
    return WiFi.status() == WL_CONNECTED;
  }

  static IPAddress localIp() {
    return WiFi.localIP();
  }

private:
  static bool parseIp(const char* text, IPAddress& address) {
    return text != nullptr && address.fromString(text);
  }

  static bool configureStaticIp(const WiFiNetworkConfig& config) {
    IPAddress localIp;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns;
    if (!parseIp(config.localIp, localIp) ||
        !parseIp(config.gateway, gateway) ||
        !parseIp(config.subnet, subnet) ||
        !parseIp(config.dns, dns)) {
      Serial.println("[E] Invalid static WiFi IP configuration");
      return false;
    }
    if (!WiFi.config(localIp, gateway, subnet, dns)) {
      Serial.println("[E] WiFi static IP configuration failed");
      return false;
    }
    return true;
  }
};

}  // namespace bacnet_example
