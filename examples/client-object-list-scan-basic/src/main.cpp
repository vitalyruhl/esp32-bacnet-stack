// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <Arduino.h>
#include <EspBacnet.h>

#ifndef EXAMPLE_USE_ETHERNET
#define EXAMPLE_USE_ETHERNET 0
#endif

#if EXAMPLE_USE_ETHERNET
#include <ETH.h>
#include <ExampleEthernet.h>
#else
#include <WiFi.h>
#endif

#include <cstddef>
#include <cstdint>

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#else
#include "secret/secrets.example.h"
#endif

#ifndef APP_NAME
#define APP_NAME "BACnet Basic Client"
#endif

#ifndef APP_VERSION
#define APP_VERSION "0.25.0"
#endif

#ifndef MY_USE_DHCP
#define MY_USE_DHCP true
#endif

#if EXAMPLE_USE_ETHERNET && !defined(MY_ETHERNET_IP)
#define MY_ETHERNET_IP MY_WIFI_IP
#endif

#ifndef BACNET_TARGET_DEVICE_INSTANCE
#define BACNET_TARGET_DEVICE_INSTANCE 1234
#endif

#ifndef BACNET_TARGET_ADDRESS
#define BACNET_TARGET_ADDRESS "192.0.2.101"
#endif

#ifndef BACNET_TARGET_PORT
#define BACNET_TARGET_PORT BacnetClient::kDefaultPort
#endif

namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kNetworkConnectTimeoutMs = 20000;
constexpr uint32_t kNetworkRetryDelayMs = 250;
constexpr uint32_t kReadTimeoutMs = 3000;
constexpr uint32_t kMaxObjectListEntries = 600;
constexpr size_t kMaxScanResults = 10;
constexpr uint32_t kSubscriptionFallbackPollMs = 5000;

BacnetClient bacnetClient;
BacnetScannedObject scanResults[kMaxScanResults];
BacnetDeviceSession* activeSession = nullptr;
BacnetPropertySubscription* activeSubscription = nullptr;

bool parseIp(const char* text, IPAddress& address) {
  return text != nullptr && address.fromString(text);
}

#if !EXAMPLE_USE_ETHERNET
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
#endif

bool connectNetwork() {
#if EXAMPLE_USE_ETHERNET
  const bacnet_example::EthernetConfig config{
    MY_USE_DHCP,
    MY_ETHERNET_IP,
    MY_GATEWAY_IP,
    MY_SUBNET_MASK,
    MY_DNS_IP,
  };
  if (!bacnet_example::EthernetNetwork::begin(APP_NAME, config) ||
      !bacnet_example::EthernetNetwork::waitForIp(kNetworkConnectTimeoutMs)) {
    return false;
  }
  Serial.print("[I] local Ethernet IP ");
  Serial.println(bacnet_example::EthernetNetwork::localIp());
  return true;
#else
  WiFi.mode(WIFI_STA);
  if (!configureStaticIp()) {
    return false;
  }

  Serial.println("[I] Connecting WiFi");
  WiFi.begin(MY_WIFI_SSID, MY_WIFI_PASSWORD);

  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < kNetworkConnectTimeoutMs) {
    delay(kNetworkRetryDelayMs);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[E] WiFi connection timed out");
    return false;
  }
  Serial.println("[I] WiFi connected");
  Serial.print("[I] local WiFi IP ");
  Serial.println(WiFi.localIP());
  return true;
#endif
}

void printValue(const char* label, BacnetDeviceSessionReadStatus status, const BacnetValue& value) {
  Serial.print("  ");
  Serial.print(label);
  Serial.print(": ");
  Serial.print(bacnetReadStatusText(status));
  if (status == BacnetDeviceSessionReadStatus::Ack) {
    Serial.print(" ");
    Serial.print(value.displayText());
  }
  Serial.println();
}

void printObjectId(BacnetObjectId objectId) {
  Serial.print(bacnetObjectTypeText(objectId.type));
  Serial.print(",");
  Serial.print(objectId.instance);
}

void readDeviceObjectName(BacnetDeviceSession& session) {
  Serial.println("[I] Reading Device object-name");

  BacnetValue value;
  BacnetProperty objectName =
    session.object(session.deviceObject())
      .property(BacnetPropertyId::ObjectName);
  const BacnetDeviceSessionReadStatus status =
    objectName.read(value, kReadTimeoutMs);

  printValue("device object-name", status, value);
}

bool isPresentValueSubscriptionCandidate(BacnetObjectId objectId) {
  return objectId.type == static_cast<uint16_t>(BacnetObjectType::AnalogInput) ||
         objectId.type ==
           static_cast<uint16_t>(BacnetObjectType::AnalogOutput) ||
         objectId.type ==
           static_cast<uint16_t>(BacnetObjectType::AnalogValue) ||
         objectId.type ==
           static_cast<uint16_t>(BacnetObjectType::BinaryInput) ||
         objectId.type ==
           static_cast<uint16_t>(BacnetObjectType::BinaryOutput) ||
         objectId.type ==
           static_cast<uint16_t>(BacnetObjectType::BinaryValue) ||
         objectId.type ==
           static_cast<uint16_t>(BacnetObjectType::MultiStateInput) ||
         objectId.type ==
           static_cast<uint16_t>(BacnetObjectType::MultiStateOutput) ||
         objectId.type ==
           static_cast<uint16_t>(BacnetObjectType::MultiStateValue);
}

void printSubscriptionReason(const BacnetSubscriptionNotification& notification) {
  bool printed = false;
  if (notification.firstValue) {
    Serial.print("first");
    printed = true;
  }
  if (notification.valueChanged) {
    Serial.print(printed ? ",value-changed" : "value-changed");
    printed = true;
  }
  if (notification.statusChanged) {
    Serial.print(printed ? ",status-changed" : "status-changed");
    printed = true;
  }
  if (!printed) {
    Serial.print("none");
  }
}

void onSubscriptionUpdate(
  const BacnetSubscriptionNotification& notification) {
  Serial.print("[I] subscription ");
  printObjectId(notification.objectId);
  Serial.print(" present-value status=");
  Serial.print(bacnetReadStatusText(notification.status));
  Serial.print(" reason=");
  printSubscriptionReason(notification);
  if (notification.hasValue && notification.value != nullptr) {
    Serial.print(" value=");
    Serial.print(notification.value->displayText());
  }
  Serial.println();
}

void printScanOptions(const BacnetObjectScanOptions& options,
                      size_t resultCapacity) {
  Serial.print("[I] scan options maxObjectListEntries=");
  Serial.print(options.maxObjectListEntries);
  Serial.print(" readTimeoutMs=");
  Serial.print(options.readTimeoutMs);
  Serial.print(" resultCapacity=");
  Serial.print(resultCapacity);
  Serial.print(" readObjectName=");
  Serial.print(options.readObjectName ? "yes" : "no");
  Serial.print(" readDescription=");
  Serial.print(options.readDescription ? "yes" : "no");
  Serial.print(" readPresentValue=");
  Serial.println(options.readPresentValue ? "yes" : "no");
  Serial.print("[I] scan filter objectTypes=");
  if (options.objectTypes == nullptr || options.objectTypeCount == 0) {
    Serial.println("all");
    return;
  }
  for (size_t i = 0; i < options.objectTypeCount; ++i) {
    if (i > 0) {
      Serial.print(",");
    }
    Serial.print(bacnetObjectTypeText(options.objectTypes[i]));
  }
  Serial.println();
}

void printScanResults(const BacnetObjectScanResult& scan) {
  Serial.print("[I] scan count-status=");
  Serial.print(bacnetReadStatusText(scan.objectListCountStatus));
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
    printValue("present-value", object.presentValueStatus, object.presentValue);
  }
}

bool startPresentValueSubscription(BacnetDeviceSession& session,
                                   const BacnetObjectScanResult& scan) {
  for (size_t i = 0; i < scan.stored; ++i) {
    const BacnetObjectId objectId = scanResults[i].objectId;
    if (!isPresentValueSubscriptionCandidate(objectId)) {
      continue;
    }

    BacnetSubscribeOptions options;
    options.fallbackPollMs = kSubscriptionFallbackPollMs;
    options.immediateFirstRead = true;
    options.notifyOnStatusChange = true;

    static BacnetPropertySubscription subscription =
      session.object(objectId)
        .property(BacnetPropertyId::PresentValue)
        .subscribe(onSubscriptionUpdate, nullptr, options);

    activeSession = &session;
    activeSubscription = &subscription;

    Serial.print("[I] subscribing to ");
    printObjectId(objectId);
    Serial.print(" present-value fallbackPollMs=");
    Serial.println(options.fallbackPollMs);
    return true;
  }

  Serial.println("[W] no scanned process object found for present-value "
                 "subscription");
  return false;
}

void runScan() {
  if (!bacnetClient.begin()) {
    Serial.println("[E] BACnet client failed to start");
    return;
  }
  Serial.print("[I] BACnet local UDP port ");
  Serial.println(bacnetClient.localPort());

  IPAddress targetAddress;
  if (!parseIp(BACNET_TARGET_ADDRESS, targetAddress)) {
    Serial.print("[E] Invalid BACnet target IP: ");
    Serial.println(BACNET_TARGET_ADDRESS);
    return;
  }
  static BacnetDeviceSession session = BacnetDeviceSession::fromEndpoint(
    bacnetClient, BACNET_TARGET_DEVICE_INSTANCE, targetAddress, BACNET_TARGET_PORT);

  Serial.print("[I] target BACnet IP ");
  Serial.println(targetAddress);
  Serial.print("[I] target BACnet device ID ");
  Serial.println(static_cast<uint32_t>(BACNET_TARGET_DEVICE_INSTANCE));
  Serial.print("[I] target BACnet port ");
  Serial.println(static_cast<uint16_t>(BACNET_TARGET_PORT));

  readDeviceObjectName(session);

  const BacnetObjectType valueObjectTypes[] = {
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

  BacnetObjectScanOptions options;
  bacnetSetObjectTypeFilter(options, valueObjectTypes);
  options.maxObjectListEntries = kMaxObjectListEntries;
  options.readTimeoutMs = kReadTimeoutMs;

  Serial.println("[I] Starting BACnet object-list scan");
  printScanOptions(options, kMaxScanResults);
  const BacnetObjectScanResult scan =
    session.scanObjectList(options, scanResults, kMaxScanResults);
  printScanResults(scan);
  if (scan.stored == 0) {
    Serial.println("[W] scan stored no objects; subscription validation skipped");
    return;
  }
  startPresentValueSubscription(session, scan);
}

} // namespace

void setup() {
  Serial.begin(kSerialBaud);
  delay(500);
  Serial.println();
  Serial.print("[I] demo ");
  Serial.println(APP_NAME);
  Serial.print("[I] version ");
  Serial.println(APP_VERSION);

  if (!connectNetwork()) {
    return;
  }
  runScan();
}

void loop() {
  if (activeSession != nullptr && activeSubscription != nullptr) {
    activeSession->poll(*activeSubscription, millis());
  }
  delay(25);
}
