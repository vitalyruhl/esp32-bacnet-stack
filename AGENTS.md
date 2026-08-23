# Root Agent Instructions

This file is the universal, cross-tool governance kernel. If it conflicts with
`.github/AGENTS.md`, follow the stricter rule and report the conflict.

## Governance Loading

- At task start, perform one minimal governance gate: identify task type,
  selected role, branch before edits or Git mutations, risk level, forbidden
  actions, and expected validation.
- Load only `.github/AGENTS.md`, the selected agent, and the skills required
  for the task. Read `project.agent.md` only when project paths, validation,
  version policy, architecture facts, or Level C classification are needed.
- Reuse stable context in the same session; reload only after a role or relevant
  governance change, uncertainty, or an explicit fresh audit. Never load
  unrelated agents merely because governance changed.
- When ProjectAtlas is available, read and follow its installed,
  version-matched `projectatlas` skill after the governance gate and before
  broad repository discovery. Initialize only when project-local Atlas state is
  absent, and follow the skill's MCP-first incremental freshness policy.

## Universal Safety

- User changes are sacred: never revert or overwrite them without explicit
  request.
- Do not stage, commit, push, merge, rebase, reset, clean, stash, switch
  branches, delete files, or update release branches unless explicitly
  requested or required by a named workflow.
- Do not perform direct product work on `main`/`master`; the documented
  docs-only TODO exception remains workflow-controlled.
- `main` and `server` are permanent integration branches and must never be
  deleted or renamed by cleanup or final synchronization. Client, shared,
  repository, and general work normally integrates into `main`; server-specific
  work normally integrates into `server`. Integrate `server` into `main` only
  through an explicit release/integration request.
- Do not run upload, monitor, destructive, hardware-affecting, or
  network-affecting commands without explicit request.
- Never mark an issue solved or fixed until the user confirms it works.
- Stop and report a blocker when instructions, scope, safety, or ownership are
  ambiguous.

## Language

- Use informal German in normal user chat.
- Repository artifacts, including governance, documentation, code comments,
  identifiers, errors, logs, and GitHub text, must be English.
- Preserve exact paths, commands, symbols, identifiers, names, issue/PR
  numbers, tags, versions, and quoted literals. Normalize only informal text
  used to create a new artifact when the scope remains unchanged.
