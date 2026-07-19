// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "HilResultOutput.h"

#include <Arduino.h>

namespace bacnet_hil {

void printResult(const char* status, const char* message) {
  Serial.print("[");
  Serial.print(status);
  Serial.print("] ");
  Serial.println(message);
}

void printScenarioLine(ScenarioOutcome outcome,
                       const char* id,
                       const char* scenarioName,
                       const char* detail) {
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

} // namespace bacnet_hil
