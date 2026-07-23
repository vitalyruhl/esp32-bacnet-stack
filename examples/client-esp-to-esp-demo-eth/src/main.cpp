// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

// Keep the established Ethernet client implementation unchanged. The wrapper
// provides the paired-server target, compact live diagnostics, fixed BV320
// control actions, and a dedicated persistent diagnostics namespace.
#define APP_NAME "ESP-to-ESP BACnet Client"
#define APP_VERSION "0.36.0"
#include "generated_client_base.inc"

#include <Preferences.h>
#include <esp_system.h>

#include <atomic>

namespace {

constexpr char kNvsNamespace[] = "esp2esp_cli";
constexpr uint32_t kDiagnosticsSchema = 1;
constexpr size_t kPreviewCount = kBacnetMaxFoundObjectsToDisplay;
constexpr size_t kRemoteObjectCount = 8;
constexpr uint32_t kBv320PollingStaleMs =
  kBacnetSubscriptionFallbackPollMs + 2U * kBacnetScanReadTimeoutMs;

enum class Bv320ValueState : uint8_t {
  Active,
  Inactive,
  Unavailable,
};

struct Bv320RemoteState {
  Bv320ValueState valueState = Bv320ValueState::Unavailable;
  const char* receiveMode = "Unavailable";
  uint32_t lastUpdateMs = 0;
};

Preferences diagnosticsPreferences;
uint32_t bootCount = 0;
uint32_t networkDropCount = 0;
uint32_t peerLossCount = 0;
uint32_t reconnectCount = 0;
uint32_t lastResetReason = 0;
float loopTimeMs = 0.0F;
uint32_t lastMainLoopAtMs = 0;
bool ethernetConnectedOnce = false;
bool ethernetOutageActive = false;
bool peerOutageActive = false;
uint32_t lastPeerHealthyMs = 0;
std::atomic<bool> bv320WriteInProgress{false};
String bv320CommandStatus = "No command sent";
Bv320ValueState bv320IndicatorState = Bv320ValueState::Unavailable;

void persistCounters() {
  diagnosticsPreferences.putUInt("boot", bootCount);
  diagnosticsPreferences.putUInt("netdrop", networkDropCount);
  diagnosticsPreferences.putUInt("peerloss", peerLossCount);
  diagnosticsPreferences.putUInt("reconn", reconnectCount);
  diagnosticsPreferences.putUInt("reset", lastResetReason);
}

void initializeDiagnostics() {
  diagnosticsPreferences.begin(kNvsNamespace, false);
  if (diagnosticsPreferences.getUInt("schema", 0) != kDiagnosticsSchema) {
    diagnosticsPreferences.clear();
    diagnosticsPreferences.putUInt("schema", kDiagnosticsSchema);
  }
  bootCount = diagnosticsPreferences.getUInt("boot", 0) + 1;
  networkDropCount = diagnosticsPreferences.getUInt("netdrop", 0);
  peerLossCount = diagnosticsPreferences.getUInt("peerloss", 0);
  reconnectCount = diagnosticsPreferences.getUInt("reconn", 0);
  lastResetReason = static_cast<uint32_t>(esp_reset_reason());
  persistCounters();
}

size_t activeCovSubscriptionCount() {
  size_t count = 0;
  const BacnetValueObjectPreview* groups[] = {
    analogValues, binaryValues, multiStateValues};
  for (const BacnetValueObjectPreview* group : groups) {
    for (size_t index = 0; index < kPreviewCount; ++index) {
      if (group[index].subscription &&
          group[index].subscription->covStatus() == BacnetCovSubscriptionStatus::Active) {
        ++count;
      }
    }
  }
  return count;
}

const char* covStateText(const BacnetValueObjectPreview& preview) {
  if (!preview.subscription) {
    return "not subscribed";
  }
  switch (preview.subscription->covStatus()) {
    case BacnetCovSubscriptionStatus::Active:
      return preview.usesPropertyCov ? "active property" : "active object";
    case BacnetCovSubscriptionStatus::Pending:
      return "pending";
    case BacnetCovSubscriptionStatus::Error:
      return "fallback polling (error)";
    case BacnetCovSubscriptionStatus::Reject:
      return "fallback polling (reject)";
    case BacnetCovSubscriptionStatus::Abort:
      return "fallback polling (abort)";
    case BacnetCovSubscriptionStatus::Timeout:
      return "fallback polling (timeout)";
    case BacnetCovSubscriptionStatus::SendFailed:
      return "fallback polling (send failed)";
  }
  return "unknown";
}

void appendStatusFlags(char* buffer, size_t capacity, const BacnetValueObjectPreview& preview) {
  if (preview.statusFlagsStatus != BacnetPropertyReadStatus::Ack ||
      preview.statusFlags.type != BacnetValueType::BitString) {
    std::snprintf(buffer, capacity, "unavailable");
    return;
  }
  BacnetStatusFlags flags;
  if (!bacnetDecodeStatusFlags(preview.statusFlags, flags)) {
    std::snprintf(buffer, capacity, "%s", preview.statusFlags.displayText());
    return;
  }
  std::snprintf(buffer,
                capacity,
                "alarm=%u fault=%u overridden=%u oos=%u",
                flags.inAlarm ? 1U : 0U,
                flags.fault ? 1U : 0U,
                flags.overridden ? 1U : 0U,
                flags.outOfService ? 1U : 0U);
}

const BacnetValueObjectPreview* previewAt(size_t index) {
  constexpr size_t kAnalogPreviewCount = 2;
  if (index < kAnalogPreviewCount) {
    return index < kPreviewCount ? &analogValues[index] : nullptr;
  }
  index -= kAnalogPreviewCount;
  return index < kPreviewCount ? &binaryValues[index] : nullptr;
}

const BacnetValueObjectPreview* findRemotePreview(BacnetObjectType type,
                                                  uint32_t instance) {
  const BacnetValueObjectPreview* groups[] = {
    analogValues, binaryValues, multiStateValues};
  for (const BacnetValueObjectPreview* group : groups) {
    for (size_t index = 0; index < kPreviewCount; ++index) {
      const BacnetValueObjectPreview& preview = group[index];
      if (preview.discovered && preview.object.type == static_cast<uint16_t>(type) &&
          preview.object.instance == instance) {
        return &preview;
      }
    }
  }
  return nullptr;
}

const BacnetValue* remotePresentValue(const BacnetValueObjectPreview* preview) {
  if (preview == nullptr) {
    return nullptr;
  }
  if (preview->subscription && preview->subscription->hasValue() &&
      preview->subscription->lastStatus() == BacnetDeviceSessionReadStatus::Ack) {
    return &preview->subscription->lastValue();
  }
  if (preview->scanned != nullptr &&
      preview->scanned->presentValueStatus == BacnetDeviceSessionReadStatus::Ack) {
    return &preview->scanned->presentValue;
  }
  return nullptr;
}

bool bacnetValueIsActive(const BacnetValue& value, bool& active) {
  switch (value.type) {
    case BacnetValueType::Boolean:
      active = value.booleanValue;
      return true;
    case BacnetValueType::Unsigned:
    case BacnetValueType::Enumerated:
      active = value.unsignedValue != 0U;
      return true;
    case BacnetValueType::Signed:
      active = value.signedValue != 0;
      return true;
    default:
      return false;
  }
}

const char* remoteReceiveMode(const BacnetValueObjectPreview* preview) {
  if (preview == nullptr || preview->subscription == nullptr) {
    return "?";
  }
  switch (preview->subscription->covStatus()) {
    case BacnetCovSubscriptionStatus::Active:
      return "C";
    case BacnetCovSubscriptionStatus::Error:
    case BacnetCovSubscriptionStatus::Reject:
    case BacnetCovSubscriptionStatus::Abort:
    case BacnetCovSubscriptionStatus::Timeout:
    case BacnetCovSubscriptionStatus::SendFailed:
      return "P";
    case BacnetCovSubscriptionStatus::Pending:
      return "?";
  }
  return "?";
}

const char* bv320ValueStateText(Bv320ValueState state) {
  switch (state) {
    case Bv320ValueState::Active:
      return "ACTIVE";
    case Bv320ValueState::Inactive:
      return "INACTIVE";
    case Bv320ValueState::Unavailable:
      return "Unavailable";
  }
  return "Unavailable";
}

Bv320RemoteState bv320RemoteState() {
  const BacnetValueObjectPreview* preview =
    findRemotePreview(BacnetObjectType::BinaryValue, 320);
  if (preview == nullptr || !preview->subscription) {
    return {};
  }

  const BacnetPropertySubscription& subscription = *preview->subscription;
  if (!subscription.hasValue() ||
      subscription.lastStatus() != BacnetDeviceSessionReadStatus::Ack) {
    return {};
  }

  Bv320RemoteState result;
  result.lastUpdateMs = subscription.lastUpdateMs();
  switch (subscription.covStatus()) {
    case BacnetCovSubscriptionStatus::Active:
      result.receiveMode = "COV";
      break;
    case BacnetCovSubscriptionStatus::Error:
    case BacnetCovSubscriptionStatus::Reject:
    case BacnetCovSubscriptionStatus::Abort:
    case BacnetCovSubscriptionStatus::Timeout:
    case BacnetCovSubscriptionStatus::SendFailed:
      if (result.lastUpdateMs == 0U ||
          millis() - result.lastUpdateMs > kBv320PollingStaleMs) {
        return {};
      }
      result.receiveMode = "Polling";
      break;
    case BacnetCovSubscriptionStatus::Pending:
      return {};
  }

  bool active = false;
  if (!bacnetValueIsActive(subscription.lastValue(), active)) {
    return {};
  }
  result.valueState = active ? Bv320ValueState::Active : Bv320ValueState::Inactive;
  return result;
}

void updateBv320IndicatorStyle(Bv320ValueState state) {
  if (state == bv320IndicatorState) {
    return;
  }
  bv320IndicatorState = state;
  ConfigManager.getRuntime().updateRuntimeMeta(
    "esp2espClient", "bv320PresentValue", [state](RuntimeFieldMeta& meta) {
      meta.hasAlarm = state == Bv320ValueState::Unavailable;
      meta.boolAlarmValue = false;
      meta.alarmWhenTrue = false;
    });
}

const char* bv320WriteStatusText(BacnetDeviceSessionWriteStatus status) {
  switch (status) {
    case BacnetDeviceSessionWriteStatus::Ack:
      return "acknowledged";
    case BacnetDeviceSessionWriteStatus::Error:
      return "BACnet error";
    case BacnetDeviceSessionWriteStatus::NotCommandable:
      return "BACnet error: not commandable";
    case BacnetDeviceSessionWriteStatus::Reject:
      return "BACnet reject";
    case BacnetDeviceSessionWriteStatus::Abort:
      return "BACnet abort";
    case BacnetDeviceSessionWriteStatus::Timeout:
      return "Write timeout";
    case BacnetDeviceSessionWriteStatus::SendFailed:
      return "Write send failed";
    case BacnetDeviceSessionWriteStatus::Disabled:
      return "Write disabled";
    case BacnetDeviceSessionWriteStatus::InvalidArgument:
      return "Invalid write request";
    case BacnetDeviceSessionWriteStatus::UnsupportedValue:
      return "BACnet error: unsupported value";
    case BacnetDeviceSessionWriteStatus::Busy:
      return "Write busy";
  }
  return "Write failed";
}

bool beginBv320Write(const char* action) {
  if (bv320WriteInProgress.exchange(true)) {
    bv320CommandStatus = "Write already in progress";
    return false;
  }
  if (activeBacnetSession == nullptr || !bacnetScanFinished) {
    bv320CommandStatus = "BV320 unavailable";
    bv320WriteInProgress.store(false);
    return false;
  }
  bv320CommandStatus = action;
  bv320CommandStatus += " sending";
  return true;
}

void finishBv320Write() {
  bv320WriteInProgress.store(false);
}

void writeBv320(bool active, uint8_t priority) {
  char action[32] = {};
  std::snprintf(action,
                sizeof(action),
                "P%u %s",
                static_cast<unsigned int>(priority),
                active ? "ACTIVE" : "INACTIVE");
  if (!beginBv320Write(action)) {
    return;
  }

  BacnetValue value;
  value.type = BacnetValueType::Enumerated;
  value.unsignedValue = active ? 1U : 0U;
  const BacnetDeviceSessionWriteStatus status =
    activeBacnetSession->object(BacnetObjectType::BinaryValue, 320)
      .writePresentValue(value, priority, kBacnetScanReadTimeoutMs);
  bv320CommandStatus = action;
  bv320CommandStatus += " ";
  bv320CommandStatus += bv320WriteStatusText(status);
  finishBv320Write();
}

void relinquishBv320P8AndP16() {
  if (!beginBv320Write("Relinquish P8 + P16")) {
    return;
  }

  const BacnetRemoteObject object =
    activeBacnetSession->object(BacnetObjectType::BinaryValue, 320);
  const BacnetDeviceSessionWriteStatus p8Status =
    object.relinquishPresentValue(8, kBacnetScanReadTimeoutMs);
  if (p8Status != BacnetDeviceSessionWriteStatus::Ack) {
    bv320CommandStatus = "P8 relinquish ";
    bv320CommandStatus += bv320WriteStatusText(p8Status);
    finishBv320Write();
    return;
  }

  const BacnetDeviceSessionWriteStatus p16Status =
    object.relinquishPresentValue(16, kBacnetScanReadTimeoutMs);
  if (p16Status == BacnetDeviceSessionWriteStatus::Ack) {
    bv320CommandStatus = "P8 and P16 relinquished";
  } else {
    bv320CommandStatus = "P8 relinquished; P16 ";
    bv320CommandStatus += bv320WriteStatusText(p16Status);
  }
  finishBv320Write();
}

void setRemoteBinaryInput(JsonObject& data,
                          const char* key,
                          const BacnetValueObjectPreview* preview) {
  const BacnetValue* value = remotePresentValue(preview);
  bool active = false;
  if (value == nullptr || !bacnetValueIsActive(*value, active)) {
    data[key] = nullptr;
    return;
  }
  data[key] = active;
}

void setRemoteLightSensor(JsonObject& data,
                          const BacnetValueObjectPreview* preview) {
  const BacnetValue* value = remotePresentValue(preview);
  if (value == nullptr || value->type != BacnetValueType::Real) {
    data["serverLightSensor"] = "unavailable";
    return;
  }
  char formatted[24] = {};
  std::snprintf(formatted, sizeof(formatted), "%.1f %%", value->realValue);
  data["serverLightSensor"] = formatted;
}

void appendPreviewSummary(char* buffer, size_t capacity, size_t index) {
  if (buffer == nullptr || capacity == 0) {
    return;
  }
  buffer[0] = '\0';
  const BacnetValueObjectPreview* preview = previewAt(index);
  if (preview == nullptr || !preview->discovered) {
    std::snprintf(buffer, capacity, "unused");
    return;
  }
  const char* value = valueObjectPresentValueText(*preview);
  const char* label = preview->scanned != nullptr ? bacnetScannedLabelOrNull(*preview->scanned) : nullptr;
  char statusFlags[72] = {};
  appendStatusFlags(statusFlags, sizeof(statusFlags), *preview);
  const uint32_t updateAgeSeconds = preview->lastCovUpdateMs == 0
                                      ? 0
                                      : (millis() - preview->lastCovUpdateMs) / 1000U;
  std::snprintf(buffer,
                capacity,
                "%s %u:%lu PV=%s Status_Flags=%s COV=%s updates=%lu age=%lus",
                label != nullptr && label[0] != '\0' ? label : "Remote object",
                static_cast<unsigned int>(preview->object.type),
                static_cast<unsigned long>(preview->object.instance),
                value != nullptr ? value : bacnetReadStatusText(preview->presentValueStatus),
                statusFlags,
                covStateText(*preview),
                static_cast<unsigned long>(preview->covUpdateCount),
                static_cast<unsigned long>(updateAgeSeconds));
}

void fillClientLiveRuntime(JsonObject& data) {
  for (size_t index = 0; index < kRemoteObjectCount; ++index) {
    char key[16] = {};
    char value[192] = {};
    std::snprintf(key, sizeof(key), "remote%u", static_cast<unsigned int>(index));
    appendPreviewSummary(value, sizeof(value), index);
    data[key] = value;
  }

  const BacnetValueObjectPreview* lightSensor =
    findRemotePreview(BacnetObjectType::AnalogInput, 0);
  const BacnetValueObjectPreview* resetButton =
    findRemotePreview(BacnetObjectType::BinaryInput, 0);
  const BacnetValueObjectPreview* midButton =
    findRemotePreview(BacnetObjectType::BinaryInput, 1);
  const BacnetValueObjectPreview* setButton =
    findRemotePreview(BacnetObjectType::BinaryInput, 2);
  const BacnetValueObjectPreview* bv320 =
    findRemotePreview(BacnetObjectType::BinaryValue, 320);
  const Bv320RemoteState bv320State = bv320RemoteState();
  updateBv320IndicatorStyle(bv320State.valueState);

  setRemoteLightSensor(data, lightSensor);
  setRemoteBinaryInput(data, "serverResetButton", resetButton);
  setRemoteBinaryInput(data, "serverMidButton", midButton);
  setRemoteBinaryInput(data, "serverSetButton", setButton);
  setRemoteBinaryInput(data, "serverBv320", bv320);
  data["serverLightSensorMode"] = remoteReceiveMode(lightSensor);
  data["serverResetButtonMode"] = remoteReceiveMode(resetButton);
  data["serverMidButtonMode"] = remoteReceiveMode(midButton);
  data["serverSetButtonMode"] = remoteReceiveMode(setButton);
  data["serverBv320Mode"] = remoteReceiveMode(bv320);
  data["serverBv320Updates"] = bv320 != nullptr ? bv320->covUpdateCount : 0U;
  data["bv320PresentValue"] = bv320State.valueState == Bv320ValueState::Active;
  data["bv320PresentValueText"] = bv320ValueStateText(bv320State.valueState);
  data["bv320ReceiveMode"] = bv320State.receiveMode;
  if (bv320State.lastUpdateMs == 0U) {
    data["bv320ValueAge"] = "Unavailable";
  } else {
    char age[24] = {};
    std::snprintf(age,
                  sizeof(age),
                  "%lu s",
                  static_cast<unsigned long>((millis() - bv320State.lastUpdateMs) / 1000U));
    data["bv320ValueAge"] = age;
  }
  data["bv320CommandStatus"] = bv320CommandStatus;
  data["bv320WriteInProgress"] = bv320WriteInProgress.load() ? "Writing" : "Ready";

  data["bootCount"] = bootCount;
  data["resetReason"] = lastResetReason;
  data["uptimeSeconds"] = millis() / 1000U;
  data["freeHeap"] = ESP.getFreeHeap();
  data["minFreeHeap"] = ESP.getMinFreeHeap();
  data["mainLoopAgeMs"] = lastMainLoopAtMs == 0U
                            ? 0U
                            : millis() - lastMainLoopAtMs;
  if (activeBacnetSession == nullptr) {
    data["bacnetPollAgeMs"] = nullptr;
    data["covSessionState"] = "no session";
    data["covSubscribeAttempts"] = 0U;
    data["covSendFailures"] = 0U;
    data["covTimeouts"] = 0U;
  } else {
    const uint32_t lastPollMs = activeBacnetSession->lastCovPollMs();
    data["bacnetPollAgeMs"] = lastPollMs == 0U ? 0U : millis() - lastPollMs;
    char covState[32] = {};
    std::snprintf(covState,
                  sizeof(covState),
                  "active %u/%u",
                  static_cast<unsigned int>(activeCovSubscriptionCount()),
                  static_cast<unsigned int>(kRemoteObjectCount));
    data["covSessionState"] = covState;
    data["covSubscribeAttempts"] = activeBacnetSession->covSubscribeAttemptCount();
    data["covSendFailures"] = activeBacnetSession->covSendFailureCount();
    data["covTimeouts"] = activeBacnetSession->covTimeoutCount();
  }
}

void addClientLiveText(const char* key, const char* label, int order) {
  RuntimeFieldMeta meta;
  meta.sourceGroup = "esp2espClient";
  meta.key = key;
  meta.label = label;
  meta.page = "ESP-to-ESP";
  meta.card = "Remote COV Variables";
  meta.group = "Live client state";
  meta.order = order;
  meta.isString = true;
  ConfigManager.getRuntime().addRuntimeMeta(meta);
}

void addRemoteHardwareInput(const char* key,
                            const char* label,
                            int order,
                            bool isBoolean) {
  RuntimeFieldMeta meta;
  meta.sourceGroup = "esp2espClient";
  meta.key = key;
  meta.label = label;
  meta.page = "ESP-to-ESP";
  meta.card = "Server Hardware Inputs";
  meta.group = "Remote BACnet inputs";
  meta.order = order;
  meta.isBool = isBoolean;
  meta.isString = !isBoolean;
  ConfigManager.getRuntime().addRuntimeMeta(meta);
}

void addBv320ControlText(const char* key, const char* label, int order) {
  RuntimeFieldMeta meta;
  meta.sourceGroup = "esp2espClient";
  meta.key = key;
  meta.label = label;
  meta.page = "ESP-to-ESP";
  meta.card = "BV320 Remote Control";
  meta.group = "Commandable Binary Value";
  meta.order = order;
  meta.isString = true;
  ConfigManager.getRuntime().addRuntimeMeta(meta);
}

void addBv320ValueIndicator() {
  RuntimeFieldMeta meta;
  meta.sourceGroup = "esp2espClient";
  meta.key = "bv320PresentValue";
  meta.label = "BV320 Present Value";
  meta.page = "ESP-to-ESP";
  meta.card = "BV320 Remote Control";
  meta.group = "Commandable Binary Value";
  meta.order = 10;
  meta.isBool = true;
  meta.onLabel = "ACTIVE";
  meta.offLabel = "INACTIVE";
  meta.hasAlarm = true;
  meta.boolAlarmValue = false;
  meta.alarmWhenTrue = false;
  meta.style.rule("stateDotOnTrue").set("background", "#22c55e").set("border", "none");
  meta.style.rule("stateDotOnFalse").set("background", "#6b7280").set("border", "none");
  meta.style.rule("stateDotOnAlarm").set("background", "#eab308").set("border", "none");
  ConfigManager.getRuntime().addRuntimeMeta(meta);
}

void setupClientLiveUi() {
  ConfigManager.getRuntime().addRuntimeProvider("esp2espClient", fillClientLiveRuntime, 5);

  ConfigManager.addLivePage("ESP-to-ESP", 90);
  ConfigManager.addLiveCard("ESP-to-ESP", "Server Hardware Inputs", 10);
  ConfigManager.addLiveCard("ESP-to-ESP", "Remote COV Variables", 20);
  ConfigManager.addLiveCard("ESP-to-ESP", "BV320 Remote Control", 30);
  ConfigManager.addLiveCard("ESP-to-ESP", "Diagnostics", 90);

  for (size_t index = 0; index < kRemoteObjectCount; ++index) {
    char key[16] = {};
    char label[24] = {};
    std::snprintf(key, sizeof(key), "remote%u", static_cast<unsigned int>(index));
    std::snprintf(label, sizeof(label), "Remote variable %u", static_cast<unsigned int>(index + 1U));
    addClientLiveText(key, label, 200 + static_cast<int>(index));
  }
  addRemoteHardwareInput("serverLightSensor", "Light Sensor (AI0)", 10, false);
  addRemoteHardwareInput("serverLightSensorMode", "Light Sensor receive mode", 11, false);
  addRemoteHardwareInput("serverResetButton", "Reset Button (BI0)", 20, true);
  addRemoteHardwareInput("serverResetButtonMode", "Reset Button receive mode", 21, false);
  addRemoteHardwareInput("serverMidButton", "Mid Button (BI1)", 30, true);
  addRemoteHardwareInput("serverMidButtonMode", "Mid Button receive mode", 31, false);
  addRemoteHardwareInput("serverSetButton", "Set Button (BI2)", 40, true);
  addRemoteHardwareInput("serverSetButtonMode", "Set Button receive mode", 41, false);
  addRemoteHardwareInput("serverBv320", "BV320 Present_Value", 50, true);
  addRemoteHardwareInput("serverBv320Mode", "BV320 receive mode", 51, false);
  addClientLiveText("serverBv320Updates", "BV320 COV updates", 208);

  addBv320ValueIndicator();
  addBv320ControlText("bv320PresentValueText", "BV320 textual value", 11);
  addBv320ControlText("bv320ValueAge", "Last received value age", 12);
  addBv320ControlText("bv320ReceiveMode", "Receive mode", 13);
  addBv320ControlText("bv320CommandStatus", "Last command", 40);
  addBv320ControlText("bv320WriteInProgress", "Write state", 41);
  auto bv320Controls = ConfigManager.liveGroup("esp2espClient")
                         .page("ESP-to-ESP", 90)
                         .card("BV320 Remote Control", 30)
                         .group("Commandable Binary Value", 1);
  bv320Controls.button("bv320_set_p16", "SET P16", []() { writeBv320(true, 16); }).order(20);
  bv320Controls.button("bv320_reset_p16", "RESET P16", []() { writeBv320(false, 16); }).order(21);
  bv320Controls.button("bv320_set_p8", "SET P8", []() { writeBv320(true, 8); }).order(22);
  bv320Controls.button("bv320_reset_p8", "RESET P8", []() { writeBv320(false, 8); }).order(23);
  bv320Controls
    .button("bv320_relinquish", "RELINQUISH P8 + P16", []() { relinquishBv320P8AndP16(); })
    .order(30);
  addClientLiveText("bootCount", "Boot count (NVS)", 10);
  addClientLiveText("resetReason", "Current reset reason", 11);
  addClientLiveText("uptimeSeconds", "Uptime", 12);
  addClientLiveText("freeHeap", "Free heap", 13);
  addClientLiveText("minFreeHeap", "Minimum free heap", 14);
  addClientLiveText("mainLoopAgeMs", "Main loop age", 15);
  addClientLiveText("bacnetPollAgeMs", "BACnet poll age", 16);
  addClientLiveText("covSessionState", "COV state", 17);
  addClientLiveText("covSubscribeAttempts", "COV subscribe/renew attempts", 18);
  addClientLiveText("covSendFailures", "COV local send failures", 19);
  addClientLiveText("covTimeouts", "COV timeouts", 20);

  auto diagnostics = ConfigManager.liveGroup("esp2espClient")
                       .page("ESP-to-ESP", 90)
                       .card("Diagnostics", 90);
  diagnostics.value("loopTime", []() { return loopTimeMs; })
    .label("Loop time")
    .unit("ms")
    .precision(3)
    .order(10);
}

void updateClientDiagnostics() {
  const bool ethernetUp = bacnet_example::EthernetNetwork::hasIp();
  if (ethernetUp) {
    if (ethernetConnectedOnce && ethernetOutageActive) {
      ++reconnectCount;
      ethernetOutageActive = false;
      persistCounters();
    }
    ethernetConnectedOnce = true;
  } else if (ethernetConnectedOnce && !ethernetOutageActive) {
    ++networkDropCount;
    ethernetOutageActive = true;
    persistCounters();
  }

  const bool peerHealthy = activeBacnetSession != nullptr && bacnetScanFinished;
  if (peerHealthy) {
    lastPeerHealthyMs = millis();
    if (peerOutageActive) {
      ++reconnectCount;
      peerOutageActive = false;
      persistCounters();
    }
  } else if (lastPeerHealthyMs != 0 && !peerOutageActive &&
             millis() - lastPeerHealthyMs >= 30000U) {
    ++peerLossCount;
    peerOutageActive = true;
    persistCounters();
  }
}

} // namespace

void setup() {
  initializeDiagnostics();
  espToEspBaseSetup();
  setupClientLiveUi();
}

void loop() {
  const uint32_t startedAtUs = micros();
  espToEspBaseLoop();
  updateClientDiagnostics();
  loopTimeMs = static_cast<float>(micros() - startedAtUs) / 1000.0F;
  lastMainLoopAtMs = millis();
}
