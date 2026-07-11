# Governance Agent Runtime

The model can be fixed in agent frontmatter, but reasoning level is selected at
runtime by the human operating the agent.

| Work | Recommended selection |
| --- | --- |
| Routine plan | Terra, Medium |
| Architecture or Level C plan | Terra, High |
| Normal refactor | GPT-5.3-Codex, Medium |
| Difficult multi-module refactor | GPT-5.3-Codex, High |
| Stuck refactor escalation | Terra, Medium or High only when justified |
| Implementation audit | GPT-5.3-Codex, High |
| Architecture audit | Terra, High |
| Workflow coordinator | Terra, Medium |
| Unusual merge, bypass, or conflict workflow | Terra, High |
| Luna helpers | Default or Low |