// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "HilScenarioRunner.h"

namespace bacnet_hil {

void printResult(const char* status, const char* message);
void printScenarioLine(ScenarioOutcome outcome,
                       const char* id,
                       const char* scenarioName,
                       const char* detail);
void printFinalSummary(const ScenarioSummary& summary,
                       ScenarioOutcome finalOutcome);

}  // namespace bacnet_hil
