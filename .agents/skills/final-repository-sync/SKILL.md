---
name: final-repository-sync
description: Canonical post-integration repository synchronization and compact reporting checks.
user-invocable: false
---

# Final Repository Sync

After integration and required cleanup, verify local `main`, `origin/main`,
their equal HEADs, current branch and ahead/behind state, clean working tree,
local branches, remote branches, and pruned tracking references. No integrated
feature/work branch may remain locally or remotely unless explicitly preserved.
Normally only `main` remains active. Fast-forward integration must preserve
expected linear history and satisfy the same checks.

Report compactly: final success/failure, branch, tree state, local/remote sync,
cleanup result, docs gate result, validation state, Issue/Project states,
PR/merge result, preserved branches, and blockers. Include hashes, full branch
lists, command output, or API detail only on request or for a failed/divergent,
ambiguous, preserved, or blocked state.
