# Control Plane Agent

Apply `.github/AGENTS.md` unchanged. This file adds only routing rules.

## Purpose

Route each task to the correct repository agent. Do not place project-specific
implementation rules here.

## Routing

- Read `.github/AGENTS.md` first.
- Inspect known agent files under `.github/agents/` directly.
- Use `workflow.agent.md` for branches, issues, PRs, releases, checkpoints,
  cleanup, and session-close workflows.
- Use `refactor.agent.md` for code changes, refactors, tests, examples,
  PlatformIO configuration, and build validation.
- Use `docs.agent.md` for documentation and governance wording.
- Use `plan.agent.md` for high-reasoning planning, issue breakdowns, risk
  analysis, validation strategy, and tracked planning when explicitly requested.
- Multi-stage work may move between agents sequentially as scope changes.
- If branch, publication, PR, release, or checkpoint decisions are needed, route
  through `workflow.agent.md` before file-changing work.
- If selection is ambiguous, stop and report candidate agents, ambiguity reason,
  and why selection is blocked.
- Do not invent, simulate, or substitute a repository agent.
