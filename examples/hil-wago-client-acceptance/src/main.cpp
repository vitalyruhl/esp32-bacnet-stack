// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <Arduino.h>
#include <BacnetClient.h>
#include <BacnetDeviceSession.h>
#include <EspBacnet.h>
#include <WiFi.h>

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#define HIL_HAS_LOCAL_SECRETS 1
#else
#include "secret/secrets.example.h"
#define HIL_HAS_LOCAL_SECRETS 0
#endif

#ifndef MY_USE_DHCP
#define MY_USE_DHCP true
#endif

#ifndef BACNET_TARGET_IP
#ifdef BACNET_TARGET_ADDRESS
#define BACNET_TARGET_IP BACNET_TARGET_ADDRESS
#else
#define BACNET_TARGET_IP "192.168.2.101"
#endif
#endif

#ifndef BACNET_TARGET_ADDRESS
#define BACNET_TARGET_ADDRESS BACNET_TARGET_IP
#endif

#ifndef BACNET_TARGET_PORT
#define BACNET_TARGET_PORT BacnetClient::kDefaultPort
#endif

#ifndef BACNET_TARGET_DEVICE_INSTANCE
#define BACNET_TARGET_DEVICE_INSTANCE 1682101
#endif

#ifndef BACNET_EXPECT_AV_INSTANCE
#define BACNET_EXPECT_AV_INSTANCE 220
#endif

#ifndef BACNET_EXPECT_MV_INSTANCE
#define BACNET_EXPECT_MV_INSTANCE 2020
#endif

#ifndef HIL_EXPECT_MOVING_PRESENT_VALUE
#define HIL_EXPECT_MOVING_PRESENT_VALUE false
#endif

#ifndef HIL_EXPECT_COV_SUPPORTED
#define HIL_EXPECT_COV_SUPPORTED false
#endif

#ifndef HIL_ENABLE_PROCESS_PRESENT_VALUE_READS
#define HIL_ENABLE_PROCESS_PRESENT_VALUE_READS true
#endif

#ifndef HIL_ENABLE_PROCESS_STATUS_READS
#define HIL_ENABLE_PROCESS_STATUS_READS true
#endif

#ifndef HIL_AI100_ANALOG_INPUT
#define HIL_AI100_ANALOG_INPUT 100
#endif

#ifndef HIL_REQUIRE_AI100
#define HIL_REQUIRE_AI100 true
#endif

#ifndef HIL_AI101_ANALOG_INPUT
#define HIL_AI101_ANALOG_INPUT 101
#endif

#ifndef HIL_REQUIRE_AI101
#define HIL_REQUIRE_AI101 false
#endif

#ifndef HIL_AO110_ANALOG_OUTPUT
#define HIL_AO110_ANALOG_OUTPUT 110
#endif

#ifndef HIL_REQUIRE_AO110
#define HIL_REQUIRE_AO110 true
#endif

#ifndef HIL_AO111_ANALOG_OUTPUT
#define HIL_AO111_ANALOG_OUTPUT 111
#endif

#ifndef HIL_REQUIRE_AO111
#define HIL_REQUIRE_AO111 false
#endif

#ifndef HIL_AV220_ANALOG_VALUE
#define HIL_AV220_ANALOG_VALUE 220
#endif

#ifndef HIL_REQUIRE_AV220
#define HIL_REQUIRE_AV220 true
#endif

#ifndef HIL_AV221_ANALOG_VALUE
#define HIL_AV221_ANALOG_VALUE 221
#endif

#ifndef HIL_REQUIRE_AV221
#define HIL_REQUIRE_AV221 false
#endif

#ifndef HIL_BI300_BINARY_INPUT
#define HIL_BI300_BINARY_INPUT 300
#endif

#ifndef HIL_REQUIRE_BI300
#define HIL_REQUIRE_BI300 true
#endif

#ifndef HIL_BI301_BINARY_INPUT
#define HIL_BI301_BINARY_INPUT 301
#endif

#ifndef HIL_REQUIRE_BI301
#define HIL_REQUIRE_BI301 false
#endif

#ifndef HIL_BO310_BINARY_OUTPUT
#define HIL_BO310_BINARY_OUTPUT 310
#endif

#ifndef HIL_REQUIRE_BO310
#define HIL_REQUIRE_BO310 true
#endif

#ifndef HIL_BO311_BINARY_OUTPUT
#define HIL_BO311_BINARY_OUTPUT 311
#endif

#ifndef HIL_REQUIRE_BO311
#define HIL_REQUIRE_BO311 false
#endif

#ifndef HIL_BV320_BINARY_VALUE
#define HIL_BV320_BINARY_VALUE 320
#endif

#ifndef HIL_REQUIRE_BV320
#define HIL_REQUIRE_BV320 true
#endif

#ifndef HIL_BV321_BINARY_VALUE
#define HIL_BV321_BINARY_VALUE 321
#endif

#ifndef HIL_REQUIRE_BV321
#define HIL_REQUIRE_BV321 false
#endif

#ifndef HIL_MI400_MULTISTATE_INPUT
#define HIL_MI400_MULTISTATE_INPUT 400
#endif

#ifndef HIL_REQUIRE_MI400
#define HIL_REQUIRE_MI400 true
#endif

#ifndef HIL_MI401_MULTISTATE_INPUT
#define HIL_MI401_MULTISTATE_INPUT 401
#endif

#ifndef HIL_REQUIRE_MI401
#define HIL_REQUIRE_MI401 false
#endif

#ifndef HIL_MO410_MULTISTATE_OUTPUT
#define HIL_MO410_MULTISTATE_OUTPUT 410
#endif

#ifndef HIL_REQUIRE_MO410
#define HIL_REQUIRE_MO410 true
#endif

#ifndef HIL_MO411_MULTISTATE_OUTPUT
#define HIL_MO411_MULTISTATE_OUTPUT 411
#endif

#ifndef HIL_REQUIRE_MO411
#define HIL_REQUIRE_MO411 false
#endif

#ifndef HIL_MV2020_MULTISTATE_VALUE
#define HIL_MV2020_MULTISTATE_VALUE 2020
#endif

#ifndef HIL_REQUIRE_MV2020
#define HIL_REQUIRE_MV2020 true
#endif

#ifndef HIL_MV2021_MULTISTATE_VALUE
#define HIL_MV2021_MULTISTATE_VALUE 2021
#endif

#ifndef HIL_REQUIRE_MV2021
#define HIL_REQUIRE_MV2021 false
#endif

#ifndef HIL_ENABLE_WRITE_TESTS
#define HIL_ENABLE_WRITE_TESTS false
#endif

#ifndef HIL_ENABLE_PRIORITY_WRITE_TESTS
#define HIL_ENABLE_PRIORITY_WRITE_TESTS false
#endif

namespace {

constexpr uint32_t kWifiConnectTimeoutMs = 30000;
constexpr uint32_t kWifiRetryDelayMs = 250;
constexpr uint32_t kScanTimeoutMs = 90000;
constexpr uint32_t kScanReadTimeoutMs = 3000;
constexpr uint32_t kPollDelayMs = 10;
constexpr uint32_t kMaxObjectListEntries = 600;
constexpr size_t kMaxScanResults = 40;
enum class ScenarioOutcome : uint8_t {
  Pass,
  Fail,
  Skip,
};

struct ScenarioSummary {
  uint32_t total = 0;
  uint32_t pass = 0;
  uint32_t fail = 0;
  uint32_t skip = 0;
  uint32_t requiredEnabled = 0;
  uint32_t requiredFailed = 0;
};

struct S02TargetSpec {
  const char* label = "";
  BacnetObjectType type = BacnetObjectType::AnalogValue;
  uint32_t instance = 0;
  bool required = false;
};

const char* scenarioOutcomeText(ScenarioOutcome outcome);

const char* bacnetValueTypeText(BacnetValueType type) {
  switch (type) {
    case BacnetValueType::Empty:
      return "empty";
    case BacnetValueType::Null:
      return "null";
    case BacnetValueType::Boolean:
      return "boolean";
    case BacnetValueType::Unsigned:
      return "unsigned";
    case BacnetValueType::Signed:
      return "signed";
    case BacnetValueType::Real:
      return "real";
    case BacnetValueType::BitString:
      return "bit-string";
    case BacnetValueType::Enumerated:
      return "enumerated";
    case BacnetValueType::CharacterString:
      return "string";
    case BacnetValueType::ObjectIdentifier:
      return "object-id";
    case BacnetValueType::ObjectIdentifierList:
      return "object-id-list";
    case BacnetValueType::Error:
      return "error";
    case BacnetValueType::Unsupported:
      return "unsupported";
  }
  return "unknown";
}

bool isAnalogProcessObjectType(BacnetObjectType type) {
  return type == BacnetObjectType::AnalogInput ||
         type == BacnetObjectType::AnalogOutput ||
         type == BacnetObjectType::AnalogValue;
}

bool isBinaryProcessObjectType(BacnetObjectType type) {
  return type == BacnetObjectType::BinaryInput ||
         type == BacnetObjectType::BinaryOutput ||
         type == BacnetObjectType::BinaryValue;
}

bool isMultiStateProcessObjectType(BacnetObjectType type) {
  return type == BacnetObjectType::MultiStateInput ||
         type == BacnetObjectType::MultiStateOutput ||
         type == BacnetObjectType::MultiStateValue;
}

bool hasExpectedPresentValueType(BacnetObjectType type, const BacnetValue& value) {
  if (isAnalogProcessObjectType(type)) {
    return value.type == BacnetValueType::Real;
  }
  if (isBinaryProcessObjectType(type)) {
    return value.type == BacnetValueType::Enumerated;
  }
  if (isMultiStateProcessObjectType(type)) {
    return value.type == BacnetValueType::Unsigned ||
           value.type == BacnetValueType::Enumerated;
  }
  return false;
}

const char* expectedPresentValueTypeText(BacnetObjectType type) {
  if (isAnalogProcessObjectType(type)) {
    return "real";
  }
  if (isBinaryProcessObjectType(type)) {
    return "enumerated";
  }
  if (isMultiStateProcessObjectType(type)) {
    return "unsigned-or-enumerated";
  }
  return "unknown";
}

void printS02ObjectLine(ScenarioOutcome outcome, const S02TargetSpec& target, BacnetDeviceSessionReadStatus status, const BacnetValue* value, bool expectedTypeOk, bool configured) {
  Serial.print("[");
  Serial.print(scenarioOutcomeText(outcome));
  Serial.print("] S02 ");
  Serial.print(target.label);
  Serial.print(" ");
  Serial.print(bacnetObjectTypeText(target.type));
  Serial.print("(type=");
  Serial.print(static_cast<unsigned>(static_cast<uint16_t>(target.type)));
  Serial.print(")");
  Serial.print(",");
  Serial.print(target.instance);
  Serial.print(" property=present-value read-status=");
  Serial.print(bacnetReadStatusText(status));

  if (!configured) {
    Serial.print(" not-configured");
  } else if (status == BacnetDeviceSessionReadStatus::Ack && value != nullptr) {
    Serial.print(" value=");
    Serial.print(value->displayText());
    Serial.print(" type=");
    Serial.print(bacnetValueTypeText(value->type));
    if (!expectedTypeOk) {
      Serial.print(" expected=");
      Serial.print(expectedPresentValueTypeText(target.type));
    }
  }
  Serial.println();
}

BacnetClient client;
BacnetScannedObject scanResults[kMaxScanResults];
BacnetObjectListScanJob scanJob;
bool completed = false;
bool runtimeReady = false;
IPAddress targetAddress;

const char* scenarioOutcomeText(ScenarioOutcome outcome) {
  switch (outcome) {
    case ScenarioOutcome::Pass:
      return "PASS";
    case ScenarioOutcome::Fail:
      return "FAIL";
    default:
      return "SKIP";
  }
}

bool parseIp(const char* text, IPAddress& address) {
  return text != nullptr && address.fromString(text);
}

void printResult(const char* status, const char* message) {
  Serial.print("[");
  Serial.print(status);
  Serial.print("] ");
  Serial.println(message);
}

void printScenarioLine(ScenarioOutcome outcome, const char* id, const char* scenarioName, const char* detail) {
  Serial.print("[");
  Serial.print(scenarioOutcomeText(outcome));
  Serial.print("] ");
  Serial.print(id);
  Serial.print(" ");
  Serial.print(scenarioName);
  if (detail != nullptr && detail[0] != '\0') {
    Serial.print(" - ");
    Serial.print(detail);
  }
  Serial.println();
}

void recordScenario(ScenarioSummary& summary, ScenarioOutcome outcome, bool required, bool enabled) {
  ++summary.total;
  if (enabled && required) {
    ++summary.requiredEnabled;
  }
  if (outcome == ScenarioOutcome::Pass) {
    ++summary.pass;
    return;
  }
  if (outcome == ScenarioOutcome::Fail) {
    ++summary.fail;
    if (enabled && required) {
      ++summary.requiredFailed;
    }
    return;
  }
  ++summary.skip;
}

void printFinalSummary(const ScenarioSummary& summary,
                       ScenarioOutcome finalOutcome) {
  Serial.print("[HIL] summary total=");
  Serial.print(summary.total);
  Serial.print(" pass=");
  Serial.print(summary.pass);
  Serial.print(" fail=");
  Serial.print(summary.fail);
  Serial.print(" skip=");
  Serial.println(summary.skip);

  Serial.print("[HIL] required-enabled=");
  Serial.print(summary.requiredEnabled);
  Serial.print(" required-failed=");
  Serial.println(summary.requiredFailed);

  printResult(scenarioOutcomeText(finalOutcome), "client acceptance complete");
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
    printResult("FAIL", "invalid static WiFi IP configuration");
    return false;
  }
  if (!WiFi.config(localIp, gateway, subnet, dns)) {
    printResult("FAIL", "WiFi static IP configuration failed");
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

  Serial.println("[HIL] Connecting WiFi");
  WiFi.begin(MY_WIFI_SSID, MY_WIFI_PASSWORD);

  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < kWifiConnectTimeoutMs) {
    delay(kWifiRetryDelayMs);
    yield();
  }

  if (WiFi.status() != WL_CONNECTED) {
    printResult("FAIL", "WiFi connection timed out");
    return false;
  }

  printResult("PASS", "WiFi connected");
  return true;
}

bool ensureRuntimeReady() {
  if (runtimeReady) {
    return true;
  }
  if (!connectWifi()) {
    return false;
  }
  if (!client.begin()) {
    printResult("FAIL", "BACnet client begin failed");
    return false;
  }
  if (!parseIp(BACNET_TARGET_ADDRESS, targetAddress)) {
    printResult("FAIL", "invalid BACnet target address");
    return false;
  }

  Serial.print("[HIL] target=");
  Serial.print(BACNET_TARGET_ADDRESS);
  Serial.print(":");
  Serial.print(BACNET_TARGET_PORT);
  Serial.print(" device-instance=");
  Serial.println(BACNET_TARGET_DEVICE_INSTANCE);

  runtimeReady = true;
  return true;
}

bool hasSafeReadStatus(BacnetDeviceSessionReadStatus status) {
  return status == BacnetDeviceSessionReadStatus::Ack ||
         status == BacnetDeviceSessionReadStatus::Error ||
         status == BacnetDeviceSessionReadStatus::Timeout ||
         status == BacnetDeviceSessionReadStatus::SendFailed ||
         status == BacnetDeviceSessionReadStatus::Skipped;
}

bool hasSafePropertyReadStatus(BacnetPropertyReadStatus status) {
  switch (status) {
    case BacnetPropertyReadStatus::Ack:
    case BacnetPropertyReadStatus::UnsupportedProperty:
    case BacnetPropertyReadStatus::Timeout:
    case BacnetPropertyReadStatus::Error:
    case BacnetPropertyReadStatus::Reject:
    case BacnetPropertyReadStatus::Abort:
    case BacnetPropertyReadStatus::DecodeError:
    case BacnetPropertyReadStatus::EmptyValue:
    case BacnetPropertyReadStatus::UnsupportedDatatype:
    case BacnetPropertyReadStatus::ArrayIndexNotSupported:
    case BacnetPropertyReadStatus::SendFailed:
      return true;
    case BacnetPropertyReadStatus::Busy:
    case BacnetPropertyReadStatus::Skipped:
      return false;
  }

  return false;
}

bool objectFound(BacnetObjectType type, uint32_t instance, const BacnetObjectScanResult& summary) {
  const uint16_t rawType = static_cast<uint16_t>(type);
  for (size_t i = 0; i < summary.stored; ++i) {
    if (scanResults[i].objectId.type == rawType &&
        scanResults[i].objectId.instance == instance) {
      return true;
    }
  }
  return false;
}

void printSummary(const BacnetObjectScanResult& summary) {
  Serial.print("[HIL] terminal=");
  Serial.print(bacnetObjectListScanJobStatusText(scanJob.status()));
  Serial.print(" phase=");
  Serial.print(bacnetObjectListScanPhaseText(scanJob.phase()));
  Serial.print(" inspected=");
  Serial.print(summary.inspected);
  Serial.print(" found=");
  Serial.print(summary.found);
  Serial.print(" stored=");
  Serial.print(summary.stored);
  Serial.print(" truncated=");
  Serial.println(summary.truncated ? "yes" : "no");
}

bool validateOptionalPropertyStatuses(const BacnetObjectScanResult& summary) {
  for (size_t i = 0; i < summary.stored; ++i) {
    if (!hasSafeReadStatus(scanResults[i].objectNameStatus) ||
        !hasSafeReadStatus(scanResults[i].descriptionStatus) ||
        !hasSafeReadStatus(scanResults[i].presentValueStatus)) {
      return false;
    }
  }
  return true;
}

ScenarioOutcome runNonBlockingObjectListScanScenario() {
  if (!ensureRuntimeReady()) {
    return ScenarioOutcome::Fail;
  }

  BacnetDeviceSession device(client, BACNET_TARGET_DEVICE_INSTANCE, targetAddress, BACNET_TARGET_PORT);
  const BacnetObjectType valueTypes[] = {
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
  bacnetSetObjectTypeFilter(options, valueTypes);
  options.maxObjectListEntries = kMaxObjectListEntries;
  options.readTimeoutMs = kScanReadTimeoutMs;
  options.readObjectName = true;
  options.readDescription = true;
  options.readPresentValue = true;

  if (!device.beginObjectListScan(scanJob, options, scanResults, kMaxScanResults)) {
    printResult("FAIL", "beginObjectListScan failed");
    return ScenarioOutcome::Fail;
  }

  const uint32_t startedAt = millis();
  uint32_t lastIndex = scanJob.currentIndex();
  bool progressAdvanced = false;
  bool requestWasIdle = false;

  while (scanJob.isActive() && millis() - startedAt < kScanTimeoutMs) {
    device.pollObjectListScan(scanJob, millis());
    if (scanJob.currentIndex() != lastIndex) {
      progressAdvanced = true;
      lastIndex = scanJob.currentIndex();
    }
    if (!scanJob.requestInFlight()) {
      requestWasIdle = true;
    }
    delay(kPollDelayMs);
    yield();
  }

  const BacnetObjectScanResult& summary = scanJob.summary();
  printSummary(summary);

  const bool requireExpectedAv = BACNET_EXPECT_AV_INSTANCE != 0;
  const bool requireExpectedMv = BACNET_EXPECT_MV_INSTANCE != 0;
  const bool expectedAvFound =
    !requireExpectedAv ||
    objectFound(BacnetObjectType::AnalogValue, BACNET_EXPECT_AV_INSTANCE, summary);
  const bool expectedMvFound =
    !requireExpectedMv ||
    objectFound(BacnetObjectType::MultiStateValue, BACNET_EXPECT_MV_INSTANCE, summary);

  Serial.print("[HIL] expected-av=");
  Serial.print(requireExpectedAv ? (expectedAvFound ? "yes" : "no") : "skip");
  Serial.print(" expected-mv=");
  Serial.println(requireExpectedMv ? (expectedMvFound ? "yes" : "no") : "skip");

  const bool ok =
    !scanJob.isActive() &&
    scanJob.isComplete() &&
    !scanJob.isFailed() &&
    !scanJob.requestInFlight() &&
    progressAdvanced &&
    requestWasIdle &&
    summary.objectListCountStatus == BacnetDeviceSessionReadStatus::Ack &&
    summary.inspected > 0 &&
    summary.found > 0 &&
    summary.stored > 0 &&
    expectedAvFound &&
    expectedMvFound &&
    validateOptionalPropertyStatuses(summary);

  return ok ? ScenarioOutcome::Pass : ScenarioOutcome::Fail;
}

bool hasSafeObjectStatusSnapshot(const BacnetObjectStatus& status,
                                 bool required) {
  if (!hasSafePropertyReadStatus(status.presentValueStatus) ||
      !hasSafePropertyReadStatus(status.statusFlagsStatus) ||
      !hasSafePropertyReadStatus(status.eventStateStatus) ||
      !hasSafePropertyReadStatus(status.reliabilityStatus) ||
      !hasSafePropertyReadStatus(status.outOfServiceStatus)) {
    return false;
  }

  return !required || status.presentValueStatus == BacnetPropertyReadStatus::Ack;
}

void printStatusFlags(const BacnetObjectStatus& status) {
  Serial.print(" flags=");
  Serial.print(bacnetPropertyReadStatusText(status.statusFlagsStatus));
  if (status.statusFlagsStatus == BacnetPropertyReadStatus::Ack) {
    Serial.print("(");
    Serial.print(status.statusFlags.inAlarm ? "alarm" : "no-alarm");
    Serial.print(",");
    Serial.print(status.statusFlags.fault ? "fault" : "no-fault");
    Serial.print(",");
    Serial.print(status.statusFlags.overridden ? "overridden" : "normal");
    Serial.print(",");
    Serial.print(status.statusFlags.outOfService ? "oos" : "in-service");
    Serial.print(")");
  }
}

void printStatusSnapshotLine(ScenarioOutcome outcome,
                             const S02TargetSpec& target,
                             const BacnetObjectStatus& status,
                             bool configured) {
  Serial.print("[");
  Serial.print(scenarioOutcomeText(outcome));
  Serial.print("] S03 ");
  Serial.print(target.label);
  Serial.print(" ");
  Serial.print(bacnetObjectTypeText(target.type));
  Serial.print("(type=");
  Serial.print(static_cast<unsigned>(static_cast<uint16_t>(target.type)));
  Serial.print(")");
  Serial.print(",");
  Serial.print(target.instance);
  if (!configured) {
    Serial.println(" not-configured");
    return;
  }

  Serial.print(" state=");
  Serial.print(bacnetObjectHealthStateText(status.state));
  Serial.print(" pv-status=");
  Serial.print(bacnetPropertyReadStatusText(status.presentValueStatus));
  if (status.presentValueStatus == BacnetPropertyReadStatus::Ack) {
    Serial.print(" pv=");
    Serial.print(status.presentValue.displayText());
  }
  printStatusFlags(status);
  Serial.print(" event-state=");
  Serial.print(bacnetPropertyReadStatusText(status.eventStateStatus));
  if (status.eventStateStatus == BacnetPropertyReadStatus::Ack) {
    Serial.print("(");
    Serial.print(bacnetEventStateText(status.eventState));
    Serial.print(")");
  }
  Serial.print(" reliability=");
  Serial.print(bacnetPropertyReadStatusText(status.reliabilityStatus));
  if (status.reliabilityStatus == BacnetPropertyReadStatus::Ack) {
    Serial.print("(");
    Serial.print(bacnetReliabilityText(status.reliability));
    Serial.print(")");
  }
  Serial.print(" oos=");
  Serial.print(bacnetPropertyReadStatusText(status.outOfServiceStatus));
  if (status.outOfServiceStatus == BacnetPropertyReadStatus::Ack) {
    Serial.print("(");
    Serial.print(status.outOfService ? "true" : "false");
    Serial.print(")");
  }
  Serial.println();
}

ScenarioOutcome runCommonProcessPresentValueReadScenario() {
  if (!ensureRuntimeReady()) {
    return ScenarioOutcome::Fail;
  }

  BacnetDeviceSession device(client, BACNET_TARGET_DEVICE_INSTANCE, targetAddress, BACNET_TARGET_PORT);
  const S02TargetSpec targets[] = {
    {"AI100", BacnetObjectType::AnalogInput, HIL_AI100_ANALOG_INPUT, HIL_REQUIRE_AI100},
    {"AI101", BacnetObjectType::AnalogInput, HIL_AI101_ANALOG_INPUT, HIL_REQUIRE_AI101},
    {"AO110", BacnetObjectType::AnalogOutput, HIL_AO110_ANALOG_OUTPUT, HIL_REQUIRE_AO110},
    {"AO111", BacnetObjectType::AnalogOutput, HIL_AO111_ANALOG_OUTPUT, HIL_REQUIRE_AO111},
    {"AV220", BacnetObjectType::AnalogValue, HIL_AV220_ANALOG_VALUE, HIL_REQUIRE_AV220},
    {"AV221", BacnetObjectType::AnalogValue, HIL_AV221_ANALOG_VALUE, HIL_REQUIRE_AV221},
    {"BI300", BacnetObjectType::BinaryInput, HIL_BI300_BINARY_INPUT, HIL_REQUIRE_BI300},
    {"BI301", BacnetObjectType::BinaryInput, HIL_BI301_BINARY_INPUT, HIL_REQUIRE_BI301},
    {"BO310", BacnetObjectType::BinaryOutput, HIL_BO310_BINARY_OUTPUT, HIL_REQUIRE_BO310},
    {"BO311", BacnetObjectType::BinaryOutput, HIL_BO311_BINARY_OUTPUT, HIL_REQUIRE_BO311},
    {"BV320", BacnetObjectType::BinaryValue, HIL_BV320_BINARY_VALUE, HIL_REQUIRE_BV320},
    {"BV321", BacnetObjectType::BinaryValue, HIL_BV321_BINARY_VALUE, HIL_REQUIRE_BV321},
    {"MI400", BacnetObjectType::MultiStateInput, HIL_MI400_MULTISTATE_INPUT, HIL_REQUIRE_MI400},
    {"MI401", BacnetObjectType::MultiStateInput, HIL_MI401_MULTISTATE_INPUT, HIL_REQUIRE_MI401},
    {"MO410", BacnetObjectType::MultiStateOutput, HIL_MO410_MULTISTATE_OUTPUT, HIL_REQUIRE_MO410},
    {"MO411", BacnetObjectType::MultiStateOutput, HIL_MO411_MULTISTATE_OUTPUT, HIL_REQUIRE_MO411},
    {"MV2020", BacnetObjectType::MultiStateValue, HIL_MV2020_MULTISTATE_VALUE, HIL_REQUIRE_MV2020},
    {"MV2021", BacnetObjectType::MultiStateValue, HIL_MV2021_MULTISTATE_VALUE, HIL_REQUIRE_MV2021},
  };

  size_t configuredTargets = 0;
  size_t passedTargets = 0;
  size_t skippedTargets = 0;
  size_t optionalFailures = 0;
  size_t requiredFailures = 0;

  for (const S02TargetSpec& target : targets) {
    if (target.instance == 0) {
      ++skippedTargets;
      printS02ObjectLine(ScenarioOutcome::Skip, target, BacnetDeviceSessionReadStatus::Skipped, nullptr, false, false);
      continue;
    }

    ++configuredTargets;
    BacnetValue value;
    const BacnetDeviceSessionReadStatus status =
      device.object(target.type, target.instance)
        .readPresentValue(value, kScanReadTimeoutMs);
    const bool targetOk = status == BacnetDeviceSessionReadStatus::Ack &&
                          hasExpectedPresentValueType(target.type, value);
    printS02ObjectLine(targetOk ? ScenarioOutcome::Pass : ScenarioOutcome::Fail,
                       target,
                       status,
                       &value,
                       targetOk,
                       true);

    if (targetOk) {
      ++passedTargets;
    } else if (target.required) {
      ++requiredFailures;
    } else {
      ++optionalFailures;
    }
  }

  Serial.print("[HIL] S02 configured=");
  Serial.print(configuredTargets);
  Serial.print(" passed=");
  Serial.print(passedTargets);
  Serial.print(" optional-failures=");
  Serial.print(optionalFailures);
  Serial.print(" required-failures=");
  Serial.print(requiredFailures);
  Serial.print(" skipped=");
  Serial.println(skippedTargets);

  if (configuredTargets == 0) {
    printResult("I", "S02 target objects not configured");
    return ScenarioOutcome::Skip;
  }

  return (passedTargets > 0 && requiredFailures == 0)
           ? ScenarioOutcome::Pass
           : ScenarioOutcome::Fail;
}

ScenarioOutcome runCommonProcessStatusReadScenario() {
  if (!ensureRuntimeReady()) {
    return ScenarioOutcome::Fail;
  }

  BacnetDeviceSession device(client, BACNET_TARGET_DEVICE_INSTANCE, targetAddress, BACNET_TARGET_PORT);
  const S02TargetSpec targets[] = {
    {"AI100", BacnetObjectType::AnalogInput, HIL_AI100_ANALOG_INPUT, HIL_REQUIRE_AI100},
    {"AI101", BacnetObjectType::AnalogInput, HIL_AI101_ANALOG_INPUT, HIL_REQUIRE_AI101},
    {"AO110", BacnetObjectType::AnalogOutput, HIL_AO110_ANALOG_OUTPUT, HIL_REQUIRE_AO110},
    {"AO111", BacnetObjectType::AnalogOutput, HIL_AO111_ANALOG_OUTPUT, HIL_REQUIRE_AO111},
    {"AV220", BacnetObjectType::AnalogValue, HIL_AV220_ANALOG_VALUE, HIL_REQUIRE_AV220},
    {"AV221", BacnetObjectType::AnalogValue, HIL_AV221_ANALOG_VALUE, HIL_REQUIRE_AV221},
    {"BI300", BacnetObjectType::BinaryInput, HIL_BI300_BINARY_INPUT, HIL_REQUIRE_BI300},
    {"BI301", BacnetObjectType::BinaryInput, HIL_BI301_BINARY_INPUT, HIL_REQUIRE_BI301},
    {"BO310", BacnetObjectType::BinaryOutput, HIL_BO310_BINARY_OUTPUT, HIL_REQUIRE_BO310},
    {"BO311", BacnetObjectType::BinaryOutput, HIL_BO311_BINARY_OUTPUT, HIL_REQUIRE_BO311},
    {"BV320", BacnetObjectType::BinaryValue, HIL_BV320_BINARY_VALUE, HIL_REQUIRE_BV320},
    {"BV321", BacnetObjectType::BinaryValue, HIL_BV321_BINARY_VALUE, HIL_REQUIRE_BV321},
    {"MI400", BacnetObjectType::MultiStateInput, HIL_MI400_MULTISTATE_INPUT, HIL_REQUIRE_MI400},
    {"MI401", BacnetObjectType::MultiStateInput, HIL_MI401_MULTISTATE_INPUT, HIL_REQUIRE_MI401},
    {"MO410", BacnetObjectType::MultiStateOutput, HIL_MO410_MULTISTATE_OUTPUT, HIL_REQUIRE_MO410},
    {"MO411", BacnetObjectType::MultiStateOutput, HIL_MO411_MULTISTATE_OUTPUT, HIL_REQUIRE_MO411},
    {"MV2020", BacnetObjectType::MultiStateValue, HIL_MV2020_MULTISTATE_VALUE, HIL_REQUIRE_MV2020},
    {"MV2021", BacnetObjectType::MultiStateValue, HIL_MV2021_MULTISTATE_VALUE, HIL_REQUIRE_MV2021},
  };

  size_t configuredTargets = 0;
  size_t passedTargets = 0;
  size_t skippedTargets = 0;
  size_t optionalFailures = 0;
  size_t requiredFailures = 0;

  for (const S02TargetSpec& target : targets) {
    if (target.instance == 0) {
      ++skippedTargets;
      BacnetObjectStatus skippedStatus;
      printStatusSnapshotLine(ScenarioOutcome::Skip, target, skippedStatus, false);
      continue;
    }

    ++configuredTargets;
    BacnetObjectStatus status;
    device.readObjectStatus(target.type, target.instance, status, kScanReadTimeoutMs, target.required);
    const bool targetOk = hasSafeObjectStatusSnapshot(status, target.required);
    printStatusSnapshotLine(targetOk ? ScenarioOutcome::Pass
                                     : ScenarioOutcome::Fail,
                            target,
                            status,
                            true);

    if (targetOk) {
      ++passedTargets;
    } else if (target.required) {
      ++requiredFailures;
    } else {
      ++optionalFailures;
    }
  }

  Serial.print("[HIL] S03 configured=");
  Serial.print(configuredTargets);
  Serial.print(" passed=");
  Serial.print(passedTargets);
  Serial.print(" optional-failures=");
  Serial.print(optionalFailures);
  Serial.print(" required-failures=");
  Serial.print(requiredFailures);
  Serial.print(" skipped=");
  Serial.println(skippedTargets);

  if (configuredTargets == 0) {
    printResult("I", "S03 target objects not configured");
    return ScenarioOutcome::Skip;
  }

  return (passedTargets > 0 && requiredFailures == 0)
           ? ScenarioOutcome::Pass
           : ScenarioOutcome::Fail;
}

ScenarioOutcome runPropertyCacheScenario() {
  printResult("I", "property cache scenario not implemented yet");
  return ScenarioOutcome::Fail;
}

ScenarioOutcome runSubscribeAnyPropertyScenario() {
  printResult("I", "subscribe-any-property scenario not implemented yet");
  return ScenarioOutcome::Fail;
}

ScenarioOutcome runFallbackPollingValueChangeScenario() {
  printResult("I", "fallback polling value-change scenario not implemented yet");
  return ScenarioOutcome::Fail;
}

ScenarioOutcome runSubscribeCovScenario() {
  printResult("I", "SubscribeCOV scenario not implemented yet");
  return ScenarioOutcome::Fail;
}

ScenarioOutcome runWritePropertyScenario() {
  printResult("I", "WriteProperty scenario enabled but no writes executed");
  return ScenarioOutcome::Fail;
}

ScenarioOutcome runPresentValuePriorityWriteScenario() {
  printResult("I",
              "PresentValue priority write scenario enabled but no writes executed");
  return ScenarioOutcome::Fail;
}

void runScenario(ScenarioSummary& summary, const char* id, const char* scenarioName, bool enabled, bool required, ScenarioOutcome (*scenarioRunner)(), const char* skipReason) {
  if (!enabled) {
    printScenarioLine(ScenarioOutcome::Skip, id, scenarioName, skipReason);
    recordScenario(summary, ScenarioOutcome::Skip, required, false);
    return;
  }

  const ScenarioOutcome outcome = scenarioRunner();
  const char* detail = nullptr;
  if (outcome == ScenarioOutcome::Pass) {
    detail = "completed";
  } else if (outcome == ScenarioOutcome::Fail) {
    detail = "failed";
  } else {
    detail = "skipped";
  }

  printScenarioLine(outcome, id, scenarioName, detail);
  recordScenario(summary, outcome, required, true);
}

ScenarioOutcome runAcceptanceRunner() {
  ScenarioSummary summary;

#if !HIL_HAS_LOCAL_SECRETS
  printResult("SKIP", "local HIL secrets missing");
  summary.total = 1;
  summary.skip = 1;
  printFinalSummary(summary, ScenarioOutcome::Skip);
  return ScenarioOutcome::Skip;
#else
  runScenario(summary, "S01", "non-blocking object-list scan", true, true, runNonBlockingObjectListScanScenario, "disabled");

  runScenario(summary, "S02", "common process present-value reads", HIL_ENABLE_PROCESS_PRESENT_VALUE_READS, false, runCommonProcessPresentValueReadScenario, "disabled (HIL_ENABLE_PROCESS_PRESENT_VALUE_READS=false)");

  runScenario(summary, "S03", "common process object status reads", HIL_ENABLE_PROCESS_STATUS_READS, false, runCommonProcessStatusReadScenario, "disabled (HIL_ENABLE_PROCESS_STATUS_READS=false)");

  runScenario(summary, "S04", "property cache", false, false, runPropertyCacheScenario, "disabled (future scenario block)");

  runScenario(summary, "S05", "subscribe-any-property", false, false, runSubscribeAnyPropertyScenario, "disabled (future scenario block)");

  runScenario(summary, "S06", "fallback polling value change", HIL_EXPECT_MOVING_PRESENT_VALUE, false, runFallbackPollingValueChangeScenario, "disabled (HIL_EXPECT_MOVING_PRESENT_VALUE=false)");

  runScenario(summary, "S07", "SubscribeCOV where supported", HIL_EXPECT_COV_SUPPORTED, false, runSubscribeCovScenario, "disabled (HIL_EXPECT_COV_SUPPORTED=false)");

  runScenario(summary, "S08", "WriteProperty (explicit opt-in)", HIL_ENABLE_WRITE_TESTS, false, runWritePropertyScenario, "disabled (HIL_ENABLE_WRITE_TESTS=false)");

  runScenario(summary, "S09", "PresentValue priority write (explicit opt-in)", HIL_ENABLE_PRIORITY_WRITE_TESTS, false, runPresentValuePriorityWriteScenario, "disabled (HIL_ENABLE_PRIORITY_WRITE_TESTS=false)");

  ScenarioOutcome finalOutcome = ScenarioOutcome::Skip;
  if (summary.fail > 0 || summary.requiredFailed > 0) {
    finalOutcome = ScenarioOutcome::Fail;
  } else if (summary.requiredEnabled > 0) {
    finalOutcome = ScenarioOutcome::Pass;
  }

  printFinalSummary(summary, finalOutcome);
  return finalOutcome;
#endif
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);
  const ScenarioOutcome outcome = runAcceptanceRunner();
  completed = true;
  printResult(scenarioOutcomeText(outcome), "HIL acceptance final result");
}

void loop() {
  if (completed) {
    delay(1000);
  }
}
