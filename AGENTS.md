# Root Agent Instructions

This root file exists for tools that load repository-root `AGENTS.md` before
repository-specific governance.

## Canonical Governance

- `.github/AGENTS.md` is canonical generic governance.
- `.github/agents/project.agent.md` is mandatory project-specific context.
- Before repository work, read `.github/AGENTS.md`,
  `.github/agents/project.agent.md`, and the selected
  `.github/agents/*.agent.md` directly by known path.
- If this file conflicts with `.github/AGENTS.md`, follow `.github/AGENTS.md`.
- Defer workflow, branch, PR, merge, release, validation, version, tool, and
  routing rules to `.github/AGENTS.md` plus the project profile.

## Communication

- Use informal German ("du") in normal chat with the user.
- Keep user-facing summaries brief unless detail is requested.
- Repository artifacts, code comments, commits, issues, PRs, and generated
  governance text follow `.github/AGENTS.md`.

## Safety Baseline

- User changes are sacred; never revert or overwrite them without explicit
  request.
- Do not stage, commit, push, merge, rebase, reset, clean, stash, switch
  branches, delete files, or update release branches unless explicitly requested
  or required by a named workflow in `.github/AGENTS.md`.
- Do not work directly on `main` or `master` except for the documented
  docs-only TODO exception in `.github/AGENTS.md`.
- Do not run upload, monitor, destructive, hardware-affecting, or
  network-affecting commands unless explicitly requested.

## Final Rule

- Never mark an issue as solved or fixed until the user explicitly confirms it
  works.
