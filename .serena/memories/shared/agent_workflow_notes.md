# Agent Workflow Notes

- Status: workflow orientation summary only.
- This memory is not canonical governance.
- Always reread `.github/AGENTS.md`, `.github/agents/project.agent.md`, and `.github/agents/workflow.agent.md` when a full governance reload is required.
- Workflow shortcuts are governed actions, not casual shell aliases.

## Repository Orientation

- Read canonical governance directly before using repository-navigation tools.
- When available, use the installed version-matched ProjectAtlas skill as the
  first orientation layer: call one compact `atlas_session_brief`, follow its
  typed next call, and refresh changed files with `atlas_watch_once` instead of
  routine full scans. Otherwise use direct bounded discovery.
- Use Serena only after Atlas has selected the relevant file or symbol and LSP
  declarations, implementations, references, overloads, or diagnostics matter.
- Do not query both tools for the same relation unless Atlas reports material
  incomplete, ambiguous, unresolved, or truncated evidence.
- Atlas purposes describe path responsibility; Serena memories retain curated
  project knowledge. Repository files remain authoritative over both.

## Workflow Shortcuts

- `workflow.begin`: create or select the appropriate side branch, then stop before edits unless the workflow explicitly continues.
- `workflow.checkpoint`: commit and push the current coherent state after required validation or allowed validation reuse.
- `workflow.docs`: perform narrow documentation synchronization.
- `workflow.audit`: strictly read-only; no file, branch, commit, push, merge, PR, or project mutation.
- `workflow.ship`: build and verify artifacts without implicit merge.
- `workflow.ready`: prepare for review/integration and run or report validation; do not merge or update release branches.
- `workflow.toMain`: integrate client, shared, repository, and general work into `main` using the agreed repository workflow, normally PR-based; validation and documentation impact checks are required before merge.
- `workflow.toServer`: integrate server-specific work into permanent `server` using the same agreed workflow. Integrate `server` into `main` only through an explicit release/integration request.
- `workflow.cleanBranches`: preserve permanent `main` and `server`; delete only temporary branches verified as integrated into their applicable integration branch.
- `workflow.end`: report repository, validation, and blocker state without implicit Git writes.

## Shortcut Safety

- Leading-dot command tokens such as `.checkpoint`, `.audit`, `.ready`, `.toMain`, `.toServer`, and `.cleanBranches` are workflow shortcut shorthand unless clearly paths, filenames, extensions, versions, or quoted literals.
- Shortcuts run sequentially.
- Follow-up work after `workflow.audit` requires explicit user request and no remaining blockers.
- Do not bypass required checks unless explicitly confirmed and the reason is reported.
- Owner/admin bypass is explicit-only for the current action.
- Do not delete branches unless integration is verified.

Sources: `.github/AGENTS.md`, `.github/agents/workflow.agent.md`.
