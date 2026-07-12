---
name: safe-branch-cleanup
description: Canonical local, remote, and tracking-branch cleanup procedure with preservation guards.
user-invocable: false
---

# Safe Branch Cleanup

Run only when the workflow coordinator authorizes cleanup. Inspect branch,
working-tree, integration, and protection state first. Delete only local and
remote work branches proven integrated and not protected. Preserve `main`,
release branches, active branches, unmerged branches, explicitly preserved
branches, and ambiguous branches; report a reason for every preserved branch.

After authorized remote cleanup, fetch and prune tracking references, then
verify local, remote, and stale tracking state. Do not silently leave a stale or
undeletable work branch: stop or report an explicit blocker. A cleanup run after
successful `workflow.toMain` is idempotent: when no remaining branches need
action, report `already clean` and do not repeat destructive or remote work.
