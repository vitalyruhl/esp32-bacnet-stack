// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

// Keep the established Ethernet client implementation unchanged. The wrapper
// provides the paired-server target, compact live diagnostics, and a dedicated
// persistent diagnostics namespace.
#define APP_NAME "ESP-to-ESP BACnet Client"
#define APP_VERSION "0.36.0"
#include "generated_client_base.inc"

#include <Preferences.h>
#include <esp_system.h>

namespace {

constexpr char kNvsNamespace[] = "esp2esp_cli";
constexpr uint32_t kDiagnosticsSchema = 1;
constexpr size_t kPreviewCount = kBacnetMaxFoundObjectsToDisplay;
constexpr size_t kRemoteObjectCount = 7;

Preferences diagnosticsPreferences;
uint32_t bootCount = 0;
uint32_t networkDropCount = 0;
uint32_t peerLossCount = 0;
uint32_t reconnectCount = 0;
uint32_t lastResetReason = 0;
float loopTimeMs = 0.0F;
bool ethernetConnectedOnce = false;
bool ethernetOutageActive = false;
bool peerOutageActive = false;
uint32_t lastPeerHealthyMs = 0;

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

void appendStatusFlags(char* buffer, size_t capacity,
                       const BacnetValueObjectPreview& preview) {
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

  setRemoteLightSensor(data, lightSensor);
  setRemoteBinaryInput(data, "serverResetButton", resetButton);
  setRemoteBinaryInput(data, "serverMidButton", midButton);
  setRemoteBinaryInput(data, "serverSetButton", setButton);
  data["serverLightSensorMode"] = remoteReceiveMode(lightSensor);
  data["serverResetButtonMode"] = remoteReceiveMode(resetButton);
  data["serverMidButtonMode"] = remoteReceiveMode(midButton);
  data["serverSetButtonMode"] = remoteReceiveMode(setButton);
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

void setupClientLiveUi() {
  ConfigManager.getRuntime().addRuntimeProvider("esp2espClient", fillClientLiveRuntime, 5);
  auto diagnostics = ConfigManager.liveGroup("esp2espClient")
                       .page("ESP-to-ESP", 5)
                       .card("Diagnostics");
  diagnostics.value("loopTime", []() { return loopTimeMs; })
    .label("Loop time")
    .unit("ms")
    .precision(3)
    .order(10);
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
}
