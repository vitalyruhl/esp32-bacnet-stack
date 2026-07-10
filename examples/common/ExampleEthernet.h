// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <Arduino.h>
#include <ETH.h>

namespace bacnet_example {

struct EthernetConfig {
  bool useDhcp = true;
  const char* localIp = nullptr;
  const char* gateway = nullptr;
  const char* subnet = nullptr;
  const char* dns = nullptr;
};

class EthernetNetwork {
public:
  static bool begin(const char* hostname, const EthernetConfig& config) {
    hostname_ = hostname;
    hasIp_ = false;
    WiFi.onEvent(onNetworkEvent);

    if (!ETH.begin()) {
      Serial.println("[E] Ethernet PHY initialization failed");
      return false;
    }

    if (!config.useDhcp && !configureStaticIp(config)) {
      return false;
    }
    return true;
  }

  static bool waitForIp(uint32_t timeoutMs) {
    const uint32_t startedAt = millis();
    while (!hasIp_ && millis() - startedAt < timeoutMs) {
      delay(25);
      yield();
    }
    if (!hasIp_) {
      Serial.println("[E] Ethernet IP timeout");
    }
    return hasIp_;
  }

  static bool hasIp() {
    return hasIp_;
  }

  static IPAddress localIp() {
    return ETH.localIP();
  }

private:
  static bool parseIp(const char* text, IPAddress& address) {
    return text != nullptr && address.fromString(text);
  }

  static bool configureStaticIp(const EthernetConfig& config) {
    IPAddress localIp;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns;
    if (!parseIp(config.localIp, localIp) ||
        !parseIp(config.gateway, gateway) ||
        !parseIp(config.subnet, subnet) ||
        !parseIp(config.dns, dns)) {
      Serial.println("[E] Invalid static Ethernet IP configuration");
      return false;
    }
    if (!ETH.config(localIp, gateway, subnet, dns)) {
      Serial.println("[E] Ethernet static IP configuration failed");
      return false;
    }
    return true;
  }

  static void onNetworkEvent(WiFiEvent_t event) {
    switch (event) {
      case ARDUINO_EVENT_ETH_START:
        if (hostname_ != nullptr) {
          ETH.setHostname(hostname_);
        }
        Serial.println("[I] Ethernet started");
        break;
      case ARDUINO_EVENT_ETH_CONNECTED:
        Serial.println("[I] Ethernet link connected");
        break;
      case ARDUINO_EVENT_ETH_GOT_IP:
        hasIp_ = true;
        Serial.print("[I] Ethernet IP: ");
        Serial.println(ETH.localIP());
        break;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
      case ARDUINO_EVENT_ETH_LOST_IP:
        hasIp_ = false;
        Serial.println("[W] Ethernet lost IP");
        break;
#endif
      case ARDUINO_EVENT_ETH_DISCONNECTED:
        hasIp_ = false;
        Serial.println("[W] Ethernet disconnected");
        break;
      case ARDUINO_EVENT_ETH_STOP:
        hasIp_ = false;
        Serial.println("[W] Ethernet stopped");
        break;
      default:
        break;
    }
  }

  inline static volatile bool hasIp_ = false;
  inline static const char* hostname_ = nullptr;
};

} // namespace bacnet_example
