---
name: serena-index-freshness
description: Canonical Serena index refresh and reuse criteria for plan/refactor/audit tasks that use Serena.
user-invocable: false
---

# Serena Index Freshness

Load this skill only for plan, refactor, audit, or architecture-audit tasks
that actually use Serena. Do not load it for docs/governance/workflow-only
tasks unless Serena evidence is stale or the user explicitly requests it.

Refresh Serena indexing when one of these applies:

1. Relevant branch change, merge, rebase, or pull occurred.
2. New source, header, test, or library files were added.
3. PlatformIO configuration, dependencies, include paths, or compile database
   changed.
4. Serena index evidence is stale, incomplete, failed, or missing.
5. The task is a large refactor and no recent index exists for the current
   branch.

Reuse guard:

- Do not reuse Serena index evidence across branches unless identical
  commit/tree state is proven.