---
name: safe-branch-cleanup
description: Canonical local, remote, and tracking-branch cleanup procedure with preservation guards.
user-invocable: false
---

# Safe Branch Cleanup

Run only when the workflow coordinator authorizes cleanup. Inspect branch,
working-tree, integration, and protection state first. Delete only local and
remote temporary work branches proven fully integrated into their applicable
integration branch and not protected. Preserve permanent `main` and `server`,
release branches, active branches, unmerged branches, explicitly preserved
branches, and ambiguous branches; report a reason for every preserved branch.
Never select a branch for cleanup merely because it is closed, inactive, or old.
Dependabot branches are handled only by their dedicated dependency-update
workflow and are excluded from repository-refactor distribution and cleanup
decisions unless explicitly authorized.

After authorized remote cleanup, fetch and prune tracking references, then
verify local, remote, and stale tracking state. Do not silently leave a stale or
undeletable work branch: stop or report an explicit blocker. A cleanup run after
successful `workflow.toMain` or `workflow.toServer` is idempotent: when no
remaining temporary work branches need action, report `already clean` and do not
repeat destructive or remote work.
