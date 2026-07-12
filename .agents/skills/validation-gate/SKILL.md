---
name: validation-gate
description: Canonical validation ordering, autofix handling, working-tree checks, and reuse rules.
user-invocable: false
---

# Validation Gate

Load the project profile when selecting repository commands. Use enabled CI only;
if none exists, report that and use required local validation. Run relevant tests
when affected. Root builds, affected examples, affected tests, and relevant OTA
work use profile commands. Upload and monitor require explicit request.
When full pre-commit is required, run:
`pwsh -NoProfile -ExecutionPolicy Bypass -File tools/run-precommit-full.ps1`

Governance-only changes require a consistency check of routing, shortcuts,
branch/Git/PR rules, tools, Serena, validation, version, session close,
checkpoint, planning, project profile, and reporting rules.

## Final-State Ordering

1. Record `git status --short` before validation so existing user changes are
   distinguishable from validation output.
2. Run every autofix-capable command before expensive build or test commands.
   This includes pre-commit, formatters, clang-format wrappers, line-ending and
   EOF fixers, generated refreshers, and a pre-commit wrapper that couples
   cppcheck with formatting.
3. After each command that can modify files, run `git status --short` and
   compare it with the recorded baseline.
4. A non-zero command or a command that modifies files is `FAILED/AUTOFIXED`,
   not passed. Report changed files, inspect the focused diff, confirm changes
   are in scope, include them in intended scope, and rerun that command until it
   exits successfully without further changes.
5. Only then run expensive PlatformIO builds/tests. Run `git status --short`
   afterward. If files changed, stop: the expensive result does not cover the
   changed state; restart the autofix-first sequence.

Never infer success from hook names alone: exit status and post-command tree
state are required. Report indirectly covered formatter/cppcheck work as
`wrapper-covered`, not as independently executed commands. For touched C/C++
source or headers in checkpoint or integration workflows, request cppcheck via
the configured pre-commit path before expensive PlatformIO validation; report a
blocker if it cannot run.

## Reuse

Reuse validation only if it passed clearly in this session, covered the same
scope, the working tree matches the validated state, and no relevant files
changed. State: `Validation reused: <command>, passed before checkpoint, no
relevant changes since.` Never reuse after branch change without proven identical
tree, merge, rebase, conflict resolution, dependency update, PlatformIO config
change, generated refresh, or any relevant source/header/example/test/build or
metadata modification. Failed, partial, skipped, ambiguous, or insufficient
validation is never reusable.

For a GitHub Actions-only workflow dependency update, fresh successful Actions
checks on the updated workflow may validate that workflow change. Report local
PlatformIO as skipped, not reused.

For governance or documentation-only work, skip PlatformIO and state:
`PlatformIO skipped: governance/docs-only changes.`
