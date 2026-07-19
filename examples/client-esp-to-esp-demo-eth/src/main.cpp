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

Preferences diagnosticsPreferences;
uint32_t bootCount = 0;
uint32_t networkDropCount = 0;
uint32_t peerLossCount = 0;
uint32_t reconnectCount = 0;
uint32_t lastResetReason = 0;
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

size_t activeSubscriptionCount() {
  size_t count = 0;
  const BacnetValueObjectPreview* groups[] = {
    analogValues, binaryValues, multiStateValues};
  for (const BacnetValueObjectPreview* group : groups) {
    for (size_t index = 0; index < kPreviewCount; ++index) {
      if (group[index].subscription && group[index].subscription->active()) {
        ++count;
      }
    }
  }
  return count;
}

const BacnetValueObjectPreview* previewAt(size_t index) {
  const BacnetValueObjectPreview* groups[] = {
    analogValues, binaryValues, multiStateValues};
  const size_t groupIndex = index / kPreviewCount;
  const size_t memberIndex = index % kPreviewCount;
  return groupIndex < 3 ? &groups[groupIndex][memberIndex] : nullptr;
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
  std::snprintf(buffer,
                capacity,
                "%s %u:%lu PV=%s Status_Flags=%s COV=%s",
                label != nullptr && label[0] != '\0' ? label : "Remote object",
                static_cast<unsigned int>(preview->object.type),
                static_cast<unsigned long>(preview->object.instance),
                value != nullptr ? value : bacnetReadStatusText(preview->presentValueStatus),
                "not provided by property COV",
                preview->subscription && preview->subscription->active() ? "active" : "unsupported");
}

void fillClientLiveRuntime(JsonObject& data) {
  data["network"] = bacnet_example::EthernetNetwork::hasIp() ? "Ethernet connected" : "Ethernet unavailable";
  data["server"] = activeBacnetSession ? "BACnet server selected" : "BACnet server unavailable";
  data["objects"] = static_cast<uint32_t>(bacnetScanStoredObjects);
  data["subscriptions"] = static_cast<uint32_t>(activeSubscriptionCount());
  data["uptime"] = millis() / 1000U;
  data["boot"] = bootCount;
  data["netdrop"] = networkDropCount;
  data["peerloss"] = peerLossCount;
  data["reconnect"] = reconnectCount;
  data["reset"] = lastResetReason;
  for (size_t index = 0; index < kPreviewCount * 3U; ++index) {
    char key[16] = {};
    char value[192] = {};
    std::snprintf(key, sizeof(key), "remote%u", static_cast<unsigned int>(index));
    appendPreviewSummary(value, sizeof(value), index);
    data[key] = value;
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

void setupClientLiveUi() {
  ConfigManager.getRuntime().addRuntimeProvider("esp2espClient", fillClientLiveRuntime, 5);
  auto status = ConfigManager.liveGroup("esp2espClient")
                  .page("ESP-to-ESP", 5)
                  .card("Client Status");
  status.value("network", []() { return bacnet_example::EthernetNetwork::hasIp() ? "Ethernet connected" : "Ethernet unavailable"; }).label("Ethernet").order(10);
  status.value("server", []() { return activeBacnetSession ? "Selected" : "Unavailable"; }).label("BACnet server").order(20);
  status.value("objects", []() { return static_cast<uint32_t>(bacnetScanStoredObjects); }).label("Found process objects").order(30);
  status.value("subscriptions", []() { return static_cast<uint32_t>(activeSubscriptionCount()); }).label("Active subscriptions").order(40);
  status.value("uptime", []() { return millis() / 1000U; }).label("Uptime").unit("s").order(50);
  status.value("boot", []() { return bootCount; }).label("Boots/restarts").order(60);
  status.value("netdrop", []() { return networkDropCount; }).label("Network outages").order(70);
  status.value("peerloss", []() { return peerLossCount; }).label("Peer/session losses").order(80);
  status.value("reconnect", []() { return reconnectCount; }).label("Reconnects/resubscriptions").order(90);
  status.value("reset", []() { return lastResetReason; }).label("Last reset reason").order(100);
  for (size_t index = 0; index < kPreviewCount * 3U; ++index) {
    char key[16] = {};
    char label[24] = {};
    std::snprintf(key, sizeof(key), "remote%u", static_cast<unsigned int>(index));
    std::snprintf(label, sizeof(label), "Remote variable %u", static_cast<unsigned int>(index + 1U));
    addClientLiveText(key, label, 200 + static_cast<int>(index));
  }
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
  espToEspBaseLoop();
  updateClientDiagnostics();
}
