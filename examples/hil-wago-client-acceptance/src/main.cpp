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
      BacnetObjectType::AnalogValue,
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
  printResult("I", "property-list discovery scenario not implemented yet");
  return ScenarioOutcome::Fail;
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
              false, false, runPropertyListSafeReadAllScenario,
              "disabled (future scenario block)");

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
