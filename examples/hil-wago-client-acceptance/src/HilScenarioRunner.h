// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <cstdint>

namespace bacnet_hil {

enum class ScenarioOutcome : uint8_t { Pass,
                                       Fail,
                                       Skip };

struct ScenarioSummary {
  uint32_t total = 0;
  uint32_t pass = 0;
  uint32_t fail = 0;
  uint32_t skip = 0;
  uint32_t requiredEnabled = 0;
  uint32_t requiredFailed = 0;
};

using ScenarioFunction = ScenarioOutcome (*)();

const char* scenarioOutcomeText(ScenarioOutcome outcome);
void recordScenario(ScenarioSummary& summary,
                    ScenarioOutcome outcome,
                    bool required,
                    bool enabled);
void runScenario(ScenarioSummary& summary,
                 const char* id,
                 const char* scenarioName,
                 bool enabled,
                 bool required,
                 ScenarioFunction scenarioRunner,
                 const char* skipReason);

} // namespace bacnet_hil
