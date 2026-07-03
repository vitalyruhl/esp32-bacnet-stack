# Agent Workflow Notes

- Status: workflow orientation summary only.
- This memory is not canonical governance.
- Always reread `.github/AGENTS.md`, `.github/agents/project.agent.md`, and `.github/agents/workflow.agent.md` when a full governance reload is required.
- Workflow shortcuts are governed actions, not casual shell aliases.

## Workflow Shortcuts

- `workflow.begin`: create or select the appropriate side branch, then stop before edits unless the workflow explicitly continues.
- `workflow.checkpoint`: commit and push the current coherent state after required validation or allowed validation reuse.
- `workflow.docs`: perform narrow documentation synchronization.
- `workflow.audit`: strictly read-only; no file, branch, commit, push, merge, PR, or project mutation.
- `workflow.ship`: build and verify artifacts without implicit merge.
- `workflow.ready`: prepare for review/integration and run or report validation; do not merge or update release branches.
- `workflow.toMain`: integrate using the agreed repository workflow, normally PR-based; validation and documentation impact checks are required before merge.
- `workflow.cleanBranches`: delete only branches verified as integrated.
- `workflow.end`: report repository, validation, and blocker state without implicit Git writes.

## Shortcut Safety

- Leading-dot command tokens such as `.checkpoint`, `.audit`, `.ready`, `.toMain`, and `.cleanBranches` are workflow shortcut shorthand unless clearly paths, filenames, extensions, versions, or quoted literals.
- Shortcuts run sequentially.
- Follow-up work after `workflow.audit` requires explicit user request and no remaining blockers.
- Do not bypass required checks unless explicitly confirmed and the reason is reported.
- Owner/admin bypass is explicit-only for the current action.
- Do not delete branches unless integration is verified.

Sources: `.github/AGENTS.md`, `.github/agents/workflow.agent.md`.
