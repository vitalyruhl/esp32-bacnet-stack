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
#define BACNET_EXPECT_AV_INSTANCE 200
#endif

#ifndef BACNET_EXPECT_MV_INSTANCE
#define BACNET_EXPECT_MV_INSTANCE 2000
#endif

#ifndef HIL_EXPECT_MOVING_PRESENT_VALUE
#define HIL_EXPECT_MOVING_PRESENT_VALUE false
#endif

#ifndef HIL_EXPECT_COV_SUPPORTED
#define HIL_EXPECT_COV_SUPPORTED false
#endif

#ifndef HIL_ENABLE_PROPERTY_LIST_READ_ALL
#define HIL_ENABLE_PROPERTY_LIST_READ_ALL false
#endif

#ifndef HIL_READ_ALL_TARGET_OBJECT_TYPE
#define HIL_READ_ALL_TARGET_OBJECT_TYPE BacnetObjectType::AnalogValue
#endif

#ifndef HIL_READ_ALL_TARGET_OBJECT_INSTANCE
#define HIL_READ_ALL_TARGET_OBJECT_INSTANCE BACNET_EXPECT_AV_INSTANCE
#endif

#ifndef HIL_S02_MV_INSTANCE
#define HIL_S02_MV_INSTANCE BACNET_EXPECT_MV_INSTANCE
#endif

#ifndef HIL_S02_MV_REQUIRED
#define HIL_S02_MV_REQUIRED false
#endif

#ifndef HIL_S02_AI_INSTANCE
#define HIL_S02_AI_INSTANCE 0
#endif

#ifndef HIL_S02_AI_REQUIRED
#define HIL_S02_AI_REQUIRED false
#endif

#ifndef HIL_S02_AO_INSTANCE
#define HIL_S02_AO_INSTANCE 0
#endif

#ifndef HIL_S02_AO_REQUIRED
#define HIL_S02_AO_REQUIRED false
#endif

#ifndef HIL_S02_BI_INSTANCE
#define HIL_S02_BI_INSTANCE 0
#endif

#ifndef HIL_S02_BI_REQUIRED
#define HIL_S02_BI_REQUIRED false
#endif

#ifndef HIL_S02_BO_INSTANCE
#define HIL_S02_BO_INSTANCE 0
#endif

#ifndef HIL_S02_BO_REQUIRED
#define HIL_S02_BO_REQUIRED false
#endif

#ifndef HIL_S02_BV_INSTANCE
#define HIL_S02_BV_INSTANCE 0
#endif

#ifndef HIL_S02_BV_REQUIRED
#define HIL_S02_BV_REQUIRED false
#endif

#ifndef HIL_S02_MI_INSTANCE
#define HIL_S02_MI_INSTANCE 0
#endif

#ifndef HIL_S02_MI_REQUIRED
#define HIL_S02_MI_REQUIRED false
#endif

#ifndef HIL_S02_MO_INSTANCE
#define HIL_S02_MO_INSTANCE 0
#endif

#ifndef HIL_S02_MO_REQUIRED
#define HIL_S02_MO_REQUIRED false
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
constexpr size_t kMaxScanResults = 32;
constexpr size_t kMaxPropertyListEntries = 24;
constexpr size_t kMaxReadAllResults = 24;

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

void printScenarioLine(ScenarioOutcome outcome, const char* id,
                       const char* scenarioName, const char* detail) {
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

void recordScenario(ScenarioSummary& summary, ScenarioOutcome outcome,
                    bool required, bool enabled) {
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

bool objectFound(BacnetObjectType type, uint32_t instance,
                 const BacnetObjectScanResult& summary) {
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

  BacnetDeviceSession device(client, BACNET_TARGET_DEVICE_INSTANCE, targetAddress,
                             BACNET_TARGET_PORT);
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

  if (!device.beginObjectListScan(scanJob, options, scanResults,
                                  kMaxScanResults)) {
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

  const bool expectedAvFound =
      objectFound(BacnetObjectType::AnalogValue, BACNET_EXPECT_AV_INSTANCE,
                  summary);
  const bool expectedMvFound =
      objectFound(BacnetObjectType::MultiStateValue, BACNET_EXPECT_MV_INSTANCE,
                  summary);

  Serial.print("[HIL] expected-av=");
  Serial.print(expectedAvFound ? "yes" : "no");
  Serial.print(" expected-mv=");
  Serial.println(expectedMvFound ? "yes" : "no");

  bool ok = true;
  ok = ok && !scanJob.isActive();
  ok = ok && scanJob.isComplete();
  ok = ok && !scanJob.isFailed();
  ok = ok && !scanJob.requestInFlight();
  ok = ok && progressAdvanced;
  ok = ok && requestWasIdle;
  ok = ok && summary.objectListCountStatus == BacnetDeviceSessionReadStatus::Ack;
  ok = ok && summary.inspected > 0;
  ok = ok && summary.found > 0;
  ok = ok && summary.stored > 0;
  ok = ok && expectedAvFound;
  ok = ok && expectedMvFound;
  ok = ok && validateOptionalPropertyStatuses(summary);

  return ok ? ScenarioOutcome::Pass : ScenarioOutcome::Fail;
}

ScenarioOutcome runPropertyListSafeReadAllScenario() {
  if (!ensureRuntimeReady()) {
    return ScenarioOutcome::Fail;
  }

  BacnetDeviceSession device(client, BACNET_TARGET_DEVICE_INSTANCE, targetAddress,
                             BACNET_TARGET_PORT);
  const S02TargetSpec targets[] = {
      {"primary", HIL_READ_ALL_TARGET_OBJECT_TYPE,
       HIL_READ_ALL_TARGET_OBJECT_INSTANCE, HIL_READ_ALL_TARGET_OBJECT_INSTANCE != 0},
      {"multi-state-value", BacnetObjectType::MultiStateValue,
       HIL_S02_MV_INSTANCE, HIL_S02_MV_REQUIRED},
      {"analog-input", BacnetObjectType::AnalogInput, HIL_S02_AI_INSTANCE,
       HIL_S02_AI_REQUIRED},
      {"analog-output", BacnetObjectType::AnalogOutput, HIL_S02_AO_INSTANCE,
       HIL_S02_AO_REQUIRED},
      {"binary-input", BacnetObjectType::BinaryInput, HIL_S02_BI_INSTANCE,
       HIL_S02_BI_REQUIRED},
      {"binary-output", BacnetObjectType::BinaryOutput, HIL_S02_BO_INSTANCE,
       HIL_S02_BO_REQUIRED},
      {"binary-value", BacnetObjectType::BinaryValue, HIL_S02_BV_INSTANCE,
       HIL_S02_BV_REQUIRED},
      {"multi-state-input", BacnetObjectType::MultiStateInput,
       HIL_S02_MI_INSTANCE, HIL_S02_MI_REQUIRED},
      {"multi-state-output", BacnetObjectType::MultiStateOutput,
       HIL_S02_MO_INSTANCE, HIL_S02_MO_REQUIRED},
  };

  const BacnetPropertyId fallbackProperties[] = {
      BacnetPropertyId::ObjectName,
      BacnetPropertyId::Description,
      BacnetPropertyId::ObjectType,
      BacnetPropertyId::PresentValue,
  };

  size_t configuredTargets = 0;
  size_t validatedTargets = 0;
  size_t skippedTargets = 0;
  size_t optionalFailures = 0;
  size_t requiredFailures = 0;

  for (const S02TargetSpec& target : targets) {
    if (target.instance == 0) {
      ++skippedTargets;
      continue;
    }

    ++configuredTargets;
    const BacnetRemoteObject object = device.object(target.type, target.instance);
    BacnetPropertyId advertisedProperties[kMaxPropertyListEntries] = {};
    const BacnetPropertyListReadResult propertyList = object.readPropertyList(
        advertisedProperties, kMaxPropertyListEntries, kScanReadTimeoutMs);

    const BacnetPropertyId* propertiesToRead = advertisedProperties;
    size_t propertyCount = propertyList.stored;
    bool usedFallback = false;
    if (propertyList.status != BacnetPropertyReadStatus::Ack ||
        propertyCount == 0) {
      propertiesToRead = fallbackProperties;
      propertyCount = sizeof(fallbackProperties) / sizeof(fallbackProperties[0]);
      usedFallback = true;
    }

    BacnetPropertyReadResult propertyResults[kMaxReadAllResults] = {};
    const BacnetPropertyReadAllResult readAll = object.readAllProperties(
        propertiesToRead, propertyCount, propertyResults, kMaxReadAllResults,
        kScanReadTimeoutMs);

    size_t safeStatusCount = 0;
    bool presentValueAck = false;
    for (size_t i = 0; i < readAll.stored; ++i) {
      if (hasSafePropertyReadStatus(propertyResults[i].status)) {
        ++safeStatusCount;
      }
      if (propertyResults[i].propertyId == BacnetPropertyId::PresentValue &&
          propertyResults[i].status == BacnetPropertyReadStatus::Ack) {
        presentValueAck = true;
      }
    }

    Serial.print("[HIL] S02 target=");
    Serial.print(target.label);
    Serial.print(" object=");
    Serial.print(bacnetObjectTypeText(target.type));
    Serial.print(",");
    Serial.print(target.instance);
    Serial.print(" source=");
    Serial.print(usedFallback ? "fallback" : "advertised");
    Serial.print(" property-list-status=");
    Serial.print(bacnetPropertyReadStatusText(propertyList.status));
    Serial.print(" attempted=");
    Serial.print(readAll.attempted);
    Serial.print(" acked=");
    Serial.print(readAll.acked);
    Serial.print(" failed=");
    Serial.print(readAll.failed);
    Serial.print(" present-value-ack=");
    Serial.println(presentValueAck ? "yes" : "no");

    const bool targetOk = propertyCount > 0 && readAll.attempted > 1 &&
                          readAll.stored == readAll.attempted &&
                          safeStatusCount == readAll.attempted &&
                          presentValueAck &&
                          (usedFallback ||
                           propertyList.status == BacnetPropertyReadStatus::Ack);
    if (targetOk) {
      ++validatedTargets;
    } else if (target.required) {
      ++requiredFailures;
    } else {
      ++optionalFailures;
    }
  }

  Serial.print("[HIL] S02 configured=");
  Serial.print(configuredTargets);
  Serial.print(" validated=");
  Serial.print(validatedTargets);
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

  return (validatedTargets > 0 && requiredFailures == 0)
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

void runScenario(ScenarioSummary& summary, const char* id,
                 const char* scenarioName, bool enabled, bool required,
                 ScenarioOutcome (*scenarioRunner)(), const char* skipReason) {
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
  runScenario(summary, "S01", "non-blocking object-list scan", true, true,
              runNonBlockingObjectListScanScenario, "disabled");

  runScenario(summary, "S02", "property-list discovery and safe read-all",
              HIL_ENABLE_PROPERTY_LIST_READ_ALL, false,
              runPropertyListSafeReadAllScenario,
              "disabled (HIL_ENABLE_PROPERTY_LIST_READ_ALL=false)");

  runScenario(summary, "S03", "property cache", false, false,
              runPropertyCacheScenario, "disabled (future scenario block)");

  runScenario(summary, "S04", "subscribe-any-property", false, false,
              runSubscribeAnyPropertyScenario,
              "disabled (future scenario block)");

  runScenario(summary, "S05", "fallback polling value change",
              HIL_EXPECT_MOVING_PRESENT_VALUE, false,
              runFallbackPollingValueChangeScenario,
              "disabled (HIL_EXPECT_MOVING_PRESENT_VALUE=false)");

  runScenario(summary, "S06", "SubscribeCOV where supported",
              HIL_EXPECT_COV_SUPPORTED, false, runSubscribeCovScenario,
              "disabled (HIL_EXPECT_COV_SUPPORTED=false)");

  runScenario(summary, "S07", "WriteProperty (explicit opt-in)",
              HIL_ENABLE_WRITE_TESTS, false, runWritePropertyScenario,
              "disabled (HIL_ENABLE_WRITE_TESTS=false)");

  runScenario(summary, "S08", "PresentValue priority write (explicit opt-in)",
              HIL_ENABLE_PRIORITY_WRITE_TESTS, false,
              runPresentValuePriorityWriteScenario,
              "disabled (HIL_ENABLE_PRIORITY_WRITE_TESTS=false)");

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

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);
  const ScenarioOutcome outcome = runAcceptanceRunner();
  completed = true;
  if (outcome == ScenarioOutcome::Pass) {
    printResult("PASS", "HIL acceptance final result PASS");
  } else if (outcome == ScenarioOutcome::Skip) {
    printResult("SKIP", "HIL acceptance final result SKIP");
  } else {
    printResult("FAIL", "HIL acceptance final result FAIL");
  }
}

void loop() {
  if (completed) {
    delay(1000);
  }
}
