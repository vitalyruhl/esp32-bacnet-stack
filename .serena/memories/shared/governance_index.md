# Governance Index

- Status: orientation summary only.
- This memory is not canonical governance.
- Canonical governance lives in:
  - `AGENTS.md`
  - `.github/AGENTS.md`
  - `.github/agents/project.agent.md`
  - `.github/agents/<selected-agent>.agent.md`
- Serena memories, summaries, and generated snapshots never replace direct governance reads when governance reload is required.

## Required Entry Points

- Read `.github/AGENTS.md` when a full governance reload is required.
- Read `.github/agents/project.agent.md` when project-specific context is required.
- Read the selected agent file directly:
  - `.github/agents/workflow.agent.md` for branches, commits, PRs, merges, releases, checkpoints, cleanup, and session close.
  - `.github/agents/refactor.agent.md` for code, tests, examples, PlatformIO, and structural changes.
  - `.github/agents/docs.agent.md` for documentation and governance wording.
  - `.github/agents/plan.agent.md` for planning-only issue breakdowns, risk planning, validation strategy, and tracked planning.
  - `.github/agents/control-plane.agent.md` for routing only.

## High-Value Reminders

- Normal user chat is informal German.
- Repository artifacts are English: docs, governance, issues, PRs, commits, code comments, logs, identifiers, and errors.
- User changes are sacred; never revert or overwrite unrelated user changes.
- Analyze before editing.
- Do not edit `main` or `master` directly except the explicitly allowed narrow docs-only exception from governance.
- Use one side branch at a time.
- Git writes require explicit user approval or an explicitly requested governed workflow.
- Level C / risky areas require explicit confirmation.
- Upload, monitor, erase, OTA, and hardware/network-affecting commands require explicit user approval.
- Never mark an issue solved or fixed until the user confirms it works.

## Search / Discovery Reminder

- Hidden governance paths may be skipped by plain search.
- Use hidden-path-aware searches such as:
  - `rg --hidden ...`
  - `fd --hidden ...`

Sources: `AGENTS.md`, `.github/AGENTS.md`, `.github/agents/project.agent.md`, `.github/agents/*.agent.md`.