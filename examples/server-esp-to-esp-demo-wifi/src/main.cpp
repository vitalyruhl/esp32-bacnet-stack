// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

// Keep the established I/O server implementation unchanged. This wrapper adds
// only the paired-demo diagnostics and its own Preferences namespace.
#define setup espToEspBaseSetup
#define loop espToEspBaseLoop
#define onWiFiConnected espToEspBaseOnWiFiConnected
#define onWiFiDisconnected espToEspBaseOnWiFiDisconnected
#define onWiFiAPMode espToEspBaseOnWiFiAPMode
#include "../../server-io-example/src/main.cpp"
#undef onWiFiAPMode
#undef onWiFiDisconnected
#undef onWiFiConnected
#undef loop
#undef setup

#include <Preferences.h>
#include <esp_system.h>

#ifndef BACNET_DEMO_ENABLE_COV_DIAGNOSTICS
#define BACNET_DEMO_ENABLE_COV_DIAGNOSTICS 1
#endif

namespace {

constexpr char kLiveDemoAppName[] = "ESP-to-ESP BACnet Server";
constexpr char kNvsNamespace[] = "esp2esp_srv";
constexpr uint32_t kDiagnosticsSchema = 1;

Preferences diagnosticsPreferences;
uint32_t bootCount = 0;
uint32_t networkDropCount = 0;
uint32_t peerLossCount = 0;
uint32_t reconnectCount = 0;
uint32_t lastResetReason = 0;
float loopTimeMs = 0.0F;
uint32_t lastChangeMs = 0;
uint32_t changeCount = 0;
bool connectedOnce = false;
bool networkOutageActive = false;
size_t previousCovCount = 0;
bool previousLight = false;
bool previousReset = false;
bool previousMid = false;
bool previousSet = false;
bool previousLed1 = false;
bool previousLed2 = false;
uint32_t lastCovSendFailureLogMs = 0;

void observeLiveCovDiagnostic(void*, const BacnetServerCovDiagnostic& diagnostic) {
#if BACNET_DEMO_ENABLE_COV_DIAGNOSTICS
  if (diagnostic.event == BacnetServerCovDiagnosticEvent::NotificationSendFailed) {
    const uint32_t now = millis();
    if (lastCovSendFailureLogMs != 0U && now - lastCovSendFailureLogMs < 3000U) {
      return;
    }
    lastCovSendFailureLogMs = now;
  }
  printCovSubscriptionDiagnostic(diagnostic.subscription,
                                 covDiagnosticEventText(diagnostic.event));
#else
  (void)diagnostic;
#endif
}

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

uint32_t changeAgeSeconds() {
  return lastChangeMs == 0 ? 0 : (millis() - lastChangeMs) / 1000U;
}

void noteChange(bool changed) {
  if (!changed) {
    return;
  }
  lastChangeMs = millis();
  ++changeCount;
}

void updateRuntimeDiagnostics() {
  const bool lightActive = lightValue >= 0.5F;
  const bool led1Active = led1.priority.effectiveValue();
  const bool led2Active = led2.priority.effectiveValue();
  noteChange(lightActive != previousLight || resetButtonValue != previousReset ||
             midButtonValue != previousMid || setButtonValue != previousSet ||
             led1Active != previousLed1 || led2Active != previousLed2);
  previousLight = lightActive;
  previousReset = resetButtonValue;
  previousMid = midButtonValue;
  previousSet = setButtonValue;
  previousLed1 = led1Active;
  previousLed2 = led2Active;

  if (previousCovCount > 0 && covLive.count == 0) {
    ++peerLossCount;
    persistCounters();
  }
  previousCovCount = covLive.count;
}

void setupLiveDiagnosticsUi() {
  auto diagnostics = ConfigManager.liveGroup("esp2espServer")
                       .page("ESP-to-ESP", 5)
                       .card("Diagnostics");
  diagnostics.value("loopTime", []() { return loopTimeMs; })
    .label("Loop time")
    .unit("ms")
    .precision(3)
    .order(10);
}

} // namespace

void setup() {
  initializeDiagnostics();
  espToEspBaseSetup();
  bacnetServer.setCovDiagnosticListener(observeLiveCovDiagnostic);
  ConfigManager.setAppName(kLiveDemoAppName);
  ConfigManager.setAppTitle(kLiveDemoAppName);
  setupLiveDiagnosticsUi();
}

void loop() {
  const uint32_t startedAtUs = micros();
  espToEspBaseLoop();
  updateRuntimeDiagnostics();
  loopTimeMs = static_cast<float>(micros() - startedAtUs) / 1000.0F;
}

void onWiFiConnected() {
  if (connectedOnce && networkOutageActive) {
    ++reconnectCount;
    networkOutageActive = false;
    persistCounters();
  }
  connectedOnce = true;
  espToEspBaseOnWiFiConnected();
}

void onWiFiDisconnected() {
  if (connectedOnce && !networkOutageActive) {
    ++networkDropCount;
    networkOutageActive = true;
    persistCounters();
  }
  espToEspBaseOnWiFiDisconnected();
}

void onWiFiAPMode() {
  espToEspBaseOnWiFiAPMode();
}
