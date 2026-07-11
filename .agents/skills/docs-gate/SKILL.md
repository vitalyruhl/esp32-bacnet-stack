---
name: docs-gate
description: Canonical read-only semantic documentation consistency gate for pending changes.
---

# Docs Gate

Inspect only documentation relevant to the pending change: `README.md`, `docs/`,
the profile changelog when present, affected example documentation, public API
documentation/snippets, and governance documentation affected by workflow,
validation, branch, Project, release, or policy changes. Check that documented
behavior matches implementation and does not create a second conceptual model.

Report exact stale, missing, inconsistent, or contradictory findings and do not
pass until each is fixed or explicitly dispositioned by an allowed user
instruction. If none apply, return exactly:
`docs checked / no changes needed`

This gate is read-only and does not merge, commit, update Issues, or clean
branches.