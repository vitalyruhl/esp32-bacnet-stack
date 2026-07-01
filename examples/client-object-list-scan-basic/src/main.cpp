// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <Arduino.h>
#include <EspBacnet.h>
#include <WiFi.h>

#include <cstddef>
#include <cstdint>

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#else
#include "secret/secrets.example.h"
#endif

#ifndef APP_NAME
#define APP_NAME "BACnet Object List Scan Basic"
#endif

#ifndef MY_USE_DHCP
#define MY_USE_DHCP true
#endif

#ifndef BACNET_TARGET_DEVICE_INSTANCE
#define BACNET_TARGET_DEVICE_INSTANCE 1234
#endif

#ifndef BACNET_TARGET_ADDRESS_OCTETS
#define BACNET_TARGET_ADDRESS_OCTETS 192, 0, 2, 101
#endif

#ifndef BACNET_TARGET_PORT
#define BACNET_TARGET_PORT BacnetClient::kDefaultPort
#endif

namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kWifiConnectTimeoutMs = 20000;
constexpr uint32_t kWifiRetryDelayMs = 250;
constexpr uint32_t kReadTimeoutMs = 1000;
constexpr uint32_t kMaxObjectListEntries = 600;
constexpr size_t kMaxScanResults = 10;

BacnetClient bacnetClient;
BacnetScannedObject scanResults[kMaxScanResults];

bool parseIp(const char* text, IPAddress& address) {
  return text != nullptr && address.fromString(text);
}

bool configureStaticIp() {
#if MY_USE_DHCP
  return true;
#else
  IPAddress localIp;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns;
  if (!parseIp(MY_WIFI_IP, localIp) || !parseIp(MY_GATEWAY_IP, gateway) ||
      !parseIp(MY_SUBNET_MASK, subnet) || !parseIp(MY_DNS_IP, dns)) {
    Serial.println("[E] Invalid static WiFi IP configuration");
    return false;
  }
  if (!WiFi.config(localIp, gateway, subnet, dns)) {
    Serial.println("[E] WiFi static IP configuration failed");
    return false;
  }
  return true;
#endif
}

bool connectWifi() {
  WiFi.mode(WIFI_STA);
  if (!configureStaticIp()) {
    return false;
  }

  Serial.println("[I] Connecting WiFi");
  WiFi.begin(MY_WIFI_SSID, MY_WIFI_PASSWORD);

  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < kWifiConnectTimeoutMs) {
    delay(kWifiRetryDelayMs);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[E] WiFi connection timed out");
    return false;
  }
  Serial.println("[I] WiFi connected");
  return true;
}

const char* readStatusText(BacnetDeviceSessionReadStatus status) {
  switch (status) {
    case BacnetDeviceSessionReadStatus::Ack:
      return "ok";
    case BacnetDeviceSessionReadStatus::Error:
      return "error";
    case BacnetDeviceSessionReadStatus::Timeout:
      return "timeout";
    case BacnetDeviceSessionReadStatus::SendFailed:
      return "send-failed";
    case BacnetDeviceSessionReadStatus::Skipped:
      return "skipped";
  }
  return "unknown";
}

const char* objectTypeText(uint16_t objectType) {
  if (objectType == static_cast<uint16_t>(BacnetObjectType::AnalogValue)) {
    return "analog-value";
  }
  if (objectType == static_cast<uint16_t>(BacnetObjectType::MultiStateValue)) {
    return "multi-state-value";
  }
  if (objectType == static_cast<uint16_t>(BacnetObjectType::Device)) {
    return "device";
  }
  return "object";
}

void printValue(const char* label, BacnetDeviceSessionReadStatus status,
                const BacnetValue& value) {
  Serial.print("  ");
  Serial.print(label);
  Serial.print(": ");
  Serial.print(readStatusText(status));
  if (status == BacnetDeviceSessionReadStatus::Ack) {
    Serial.print(" ");
    Serial.print(value.displayText());
  }
  Serial.println();
}

void printObjectId(BacnetObjectId objectId) {
  Serial.print(objectTypeText(objectId.type));
  Serial.print(",");
  Serial.print(objectId.instance);
}

void printScanResults(const BacnetObjectScanResult& scan) {
  Serial.print("[I] scan countStatus=");
  Serial.print(readStatusText(scan.objectListCountStatus));
  Serial.print(" count=");
  Serial.print(scan.objectListCount);
  Serial.print(" inspected=");
  Serial.print(scan.inspected);
  Serial.print(" found=");
  Serial.print(scan.found);
  Serial.print(" stored=");
  Serial.print(scan.stored);
  Serial.print(" truncated=");
  Serial.println(scan.truncated ? "yes" : "no");

  for (size_t i = 0; i < scan.stored; ++i) {
    const BacnetScannedObject& object = scanResults[i];
    Serial.print("[I] object ");
    printObjectId(object.objectId);
    Serial.println();
    printValue("object-name", object.objectNameStatus, object.objectName);
    printValue("description", object.descriptionStatus, object.description);
    printValue("present-value", object.presentValueStatus,
               object.presentValue);
  }
}

void runScan() {
  if (!bacnetClient.begin()) {
    Serial.println("[E] BACnet client failed to start");
    return;
  }

  const IPAddress targetAddress(BACNET_TARGET_ADDRESS_OCTETS);
  BacnetDeviceSession session(bacnetClient, BACNET_TARGET_DEVICE_INSTANCE,
                              targetAddress, BACNET_TARGET_PORT);

  const BacnetObjectType valueObjectTypes[] = {
      BacnetObjectType::AnalogValue,
      BacnetObjectType::MultiStateValue,
  };

  BacnetObjectScanOptions options;
  options.objectTypes = valueObjectTypes;
  options.objectTypeCount = 2;
  options.maxObjectListEntries = kMaxObjectListEntries;
  options.readTimeoutMs = kReadTimeoutMs;

  Serial.println("[I] Starting BACnet object-list scan");
  const BacnetObjectScanResult scan =
      session.scanObjectList(options, scanResults, kMaxScanResults);
  printScanResults(scan);
}

}  // namespace

void setup() {
  Serial.begin(kSerialBaud);
  delay(500);
  Serial.println();
  Serial.println("[I] " APP_NAME);

  if (!connectWifi()) {
    return;
  }
  runScan();
}

void loop() {
  delay(1000);
}
