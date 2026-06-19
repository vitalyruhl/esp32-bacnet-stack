# Root Agent Instructions

Root loader for tools that read repository-root `AGENTS.md` first.

## Required Context

- `.github/AGENTS.md` is canonical generic governance.
- `.github/agents/project.agent.md` is mandatory project context.
- Before repository work, read `.github/AGENTS.md`,
  `.github/agents/project.agent.md`, and the selected
  `.github/agents/*.agent.md` directly by known path.
- If this file conflicts with `.github/AGENTS.md`, follow `.github/AGENTS.md`.

## Chat And Safety

- Use informal German ("du") in normal chat.
- Keep summaries brief unless detail is requested.
- Repository artifacts follow `.github/AGENTS.md` language rules.
- User changes are sacred; never revert or overwrite them without explicit
  request.
- Do not stage, commit, push, merge, rebase, reset, clean, stash, switch
  branches, delete files, or update release branches unless explicitly requested
  or required by a named workflow.
- Do not work directly on `main`/`master` except the documented docs-only TODO
  exception.
- Do not run upload, monitor, destructive, hardware-affecting, or
  network-affecting commands unless explicitly requested.

## Final Rule

- Never mark an issue solved/fixed until the user confirms it works.
