// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "HilScenarioRunner.h"
#include "HilResultOutput.h"

namespace bacnet_hil {

const char* scenarioOutcomeText(ScenarioOutcome outcome) {
  switch (outcome) {
    case ScenarioOutcome::Pass:
      return "PASS";
    case ScenarioOutcome::Fail:
      return "FAIL";
    case ScenarioOutcome::Skip:
      return "SKIP";
  }
  return "SKIP";
}

void recordScenario(ScenarioSummary& summary,
                    ScenarioOutcome outcome,
                    bool required,
                    bool enabled) {
  ++summary.total;
  if (enabled && required) ++summary.requiredEnabled;
  if (outcome == ScenarioOutcome::Pass) {
    ++summary.pass;
  } else if (outcome == ScenarioOutcome::Fail) {
    ++summary.fail;
    if (enabled && required) ++summary.requiredFailed;
  } else {
    ++summary.skip;
  }
}

void runScenario(ScenarioSummary& summary,
                 const char* id,
                 const char* scenarioName,
                 bool enabled,
                 bool required,
                 ScenarioFunction scenarioRunner,
                 const char* skipReason) {
  if (!enabled) {
    printScenarioLine(ScenarioOutcome::Skip, id, scenarioName, skipReason);
    recordScenario(summary, ScenarioOutcome::Skip, required, false);
    return;
  }
  const ScenarioOutcome outcome = scenarioRunner();
  const char* detail = outcome == ScenarioOutcome::Pass ? "completed"
    : outcome == ScenarioOutcome::Fail ? "failed" : "skipped";
  printScenarioLine(outcome, id, scenarioName, detail);
  recordScenario(summary, outcome, required, true);
}

}  // namespace bacnet_hil
