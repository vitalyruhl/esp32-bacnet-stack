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
  auto status = ConfigManager.liveGroup("esp2espServer")
                  .page("ESP-to-ESP", 5)
                  .card("Server Status");
  status.value("ip", []() { return WiFi.localIP().toString(); }).label("WiFi IP").order(10);
  status.boolValue("bacnet", []() { return bacnetBound; }).label("BACnet online").order(20);
  status.value("uptime", []() { return millis() / 1000U; }).label("Uptime").unit("s").order(30);
  status.value("boot", []() { return bootCount; }).label("Boots/restarts").order(40);
  status.value("netdrop", []() { return networkDropCount; }).label("Network outages").order(50);
  status.value("peerloss", []() { return peerLossCount; }).label("COV peer losses").order(60);
  status.value("reconn", []() { return reconnectCount; }).label("Reconnects").order(70);
  status.value("reset", []() { return lastResetReason; }).label("Last reset reason").order(80);

  auto process = ConfigManager.liveGroup("esp2espServer")
                   .page("ESP-to-ESP", 5)
                   .card("Local BACnet Process Objects");
  process.value("ai0", []() { return lightValue; }).label("AI0 Light Sensor").unit("%").precision(1).order(10);
  process.value("ai0flags", []() { return lightStatusFlags.value; }).label("AI0 Status_Flags").order(11);
  process.value("ai1", []() { return temperatureValue; }).label("AI1 Temperature").unit("°C").precision(2).order(20);
  process.value("ai1flags", []() { return temperatureStatusFlags.value; }).label("AI1 Status_Flags").order(21);
  process.boolValue("bi0", []() { return resetButtonValue; }).label("BI0 Reset Button").order(30);
  process.value("bi0flags", []() { return resetButtonStatusFlags.value; }).label("BI0 Status_Flags").order(31);
  process.boolValue("bi1", []() { return midButtonValue; }).label("BI1 Mid Button").order(40);
  process.value("bi1flags", []() { return midButtonStatusFlags.value; }).label("BI1 Status_Flags").order(41);
  process.boolValue("bi2", []() { return setButtonValue; }).label("BI2 Set Button").order(50);
  process.value("bi2flags", []() { return setButtonStatusFlags.value; }).label("BI2 Status_Flags").order(51);
  process.boolValue("bo0", []() { return led1.priority.effectiveValue(); }).label("BO0 LED 1").order(60);
  process.boolValue("bo1", []() { return led2.priority.effectiveValue(); }).label("BO1 LED 2").order(70);
  process.value("changeAge", []() { return changeAgeSeconds(); }).label("Last local change").unit("s ago").order(80);
  process.value("changeCount", []() { return changeCount; }).label("Local changes").order(90);
}

} // namespace

void setup() {
  initializeDiagnostics();
  espToEspBaseSetup();
  ConfigManager.setAppName(kLiveDemoAppName);
  ConfigManager.setAppTitle(kLiveDemoAppName);
  setupLiveDiagnosticsUi();
}

void loop() {
  espToEspBaseLoop();
  updateRuntimeDiagnostics();
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
