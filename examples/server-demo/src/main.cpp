// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <Arduino.h>
#include <ArduinoBacnetClient.h>
#include <ArduinoBacnetServer.h>
#include <WiFiUdp.h>

#ifndef EXAMPLE_USE_ETHERNET
#define EXAMPLE_USE_ETHERNET 0
#endif

#if EXAMPLE_USE_ETHERNET
#include <ETH.h>
#include <ExampleEthernet.h>
#else
#include <Esp32WiFiNetwork.h>
#endif

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#else
#include "secret/secrets.example.h"
#endif

#if EXAMPLE_USE_ETHERNET && !defined(MY_ETHERNET_IP)
#define MY_ETHERNET_IP MY_WIFI_IP
#endif

#include <math.h>

namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kNetworkConnectTimeoutMs = 20000;
constexpr uint32_t kNetworkRetryDelayMs = 250;
constexpr uint16_t kBacnetPort = BacnetServer::kDefaultPort;
constexpr uint32_t kDemoDeviceInstance = 1682127;
// ASHRAE-reserved; local test/example use only.
constexpr uint16_t kDemoVendorId = 555;
constexpr uint32_t kStoredValueUpdateMs = 250;
constexpr float kStoredValueOffset = 20.0F;
constexpr float kStoredValueAmplitude = 10.0F;
constexpr float kStoredValuePeriodMs = 60000.0F;
constexpr float kTwoPi = 6.28318530718F;

struct PollingDemoState {
  uint32_t startedAtMs = 0;
};

WiFiUDP bacnetUdp;
ArduinoUdpDatagramTransport bacnetTransport(bacnetUdp);
BacnetServer bacnetServer(bacnetTransport);
PollingDemoState pollingDemo;

BacnetServerAnalogValue analogValues[] = {
  {200, "AV200 Stored Sine", kStoredValueOffset, 62, false, nullptr, nullptr},
  {201, "AV201 Polling Uptime", 0.0F, 73, false, nullptr, &pollingDemo},
};

const BacnetServerDevice kDevice{
  kDemoDeviceInstance,
  kDemoVendorId,
  "ESP32 BACnet Test Server",
  "Unregistered BACnet Test Server",
  "ESP32 BACnet Server Demo",
  "0.35.0",
  "0.35.0",
};

// The portable provider ABI intentionally accepts mutable void* context.
// cppcheck-suppress constParameterCallback
float readPollingUptime(void* context) {
  const auto* state = static_cast<const PollingDemoState*>(context);
  if (state == nullptr) {
    return 0.0F;
  }
  return static_cast<float>(millis() - state->startedAtMs) / 1000.0F;
}

bool connectNetwork() {
#if EXAMPLE_USE_ETHERNET
  const bacnet_example::EthernetConfig config{
    MY_USE_DHCP,
    MY_ETHERNET_IP,
    MY_GATEWAY_IP,
    MY_SUBNET_MASK,
    MY_DNS_IP,
  };
  if (!bacnet_example::EthernetNetwork::begin("bacnet-server-demo", config) ||
      !bacnet_example::EthernetNetwork::waitForIp(kNetworkConnectTimeoutMs)) {
    return false;
  }
  Serial.print("[I] BACnet server Ethernet IP ");
  Serial.println(bacnet_example::EthernetNetwork::localIp());
  return true;
#else
  const bacnet_example::WiFiNetworkConfig config{
    MY_USE_DHCP,
    MY_WIFI_SSID,
    MY_WIFI_PASSWORD,
    MY_WIFI_IP,
    MY_GATEWAY_IP,
    MY_SUBNET_MASK,
    MY_DNS_IP,
    kNetworkConnectTimeoutMs,
    kNetworkRetryDelayMs,
    true,
  };
  return bacnet_example::Esp32WiFiNetwork::begin(config);
#endif
}

void updateStoredSine(uint32_t nowMs, bool force = false) {
  static uint32_t lastUpdateMs = 0;
  if (!force && nowMs - lastUpdateMs < kStoredValueUpdateMs) {
    return;
  }
  lastUpdateMs = nowMs;
  const float phase = kTwoPi * static_cast<float>(nowMs % 60000U) /
                      kStoredValuePeriodMs;
  analogValues[0].presentValue = kStoredValueOffset +
                                 kStoredValueAmplitude * sinf(phase);
}

void printServerIdentity() {
  Serial.print("[I] BACnet Device ID ");
  Serial.println(kDemoDeviceInstance);
  Serial.print("[I] BACnet UDP port ");
  Serial.println(kBacnetPort);
  Serial.print("[I] BACnet Vendor ID ");
  Serial.println(kDemoVendorId);
}

} // namespace

void setup() {
  Serial.begin(kSerialBaud);
  Serial.println();
  Serial.println("[I] Starting ESP32 BACnet server demo");

  if (!connectNetwork()) {
    Serial.println("[E] BACnet server network startup failed");
    return;
  }

  pollingDemo.startedAtMs = millis();
  analogValues[1].presentValueProvider = readPollingUptime;
  updateStoredSine(pollingDemo.startedAtMs, true);
  if (!bacnetServer.setAnalogValues(analogValues,
                                    sizeof(analogValues) / sizeof(analogValues[0]))) {
    Serial.println("[E] BACnet Analog Value configuration failed");
    return;
  }
  if (!bacnetServer.begin(kDevice, kBacnetPort)) {
    Serial.println("[E] BACnet server UDP startup failed");
    return;
  }

  printServerIdentity();
  Serial.println("[I] BACnet server demo started");
}

void loop() {
  if (!bacnetServer.isRunning()) {
    return;
  }
  updateStoredSine(millis());
  static_cast<void>(bacnetServer.poll());
}
