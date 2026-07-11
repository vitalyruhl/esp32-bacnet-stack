---
name: version-impact
description: Canonical version-impact classification, bump policy, scans, and synchronization requirements.
---

# Version Impact

Load the project profile for canonical version source, mirrors, and scan
commands. Classify before issue-scoped product work and report exactly one:

- `Version impact: required, expected bump: patch|minor|major, reason`
- `Version impact: not required, reason`
- `Version impact: uncertain, blocker or explicit follow-up required`

Product code, public API, PlatformIO configuration, build matrix/metadata,
build-output examples, dependencies, and release artifacts require assessment.
PlatformIO/build-matrix changes are version-impacting by default except
docs/governance-only work or the GitHub Actions-only dependency exception.

- Patch: dependency updates, bug fixes, compatible internal changes, and
  build/config maintenance without a public API break.
- Minor: compatible public features, APIs, or examples.
- Major: incompatible APIs, storage/config/schema, or migration-required
  behavior.

Before version, release, changelog, or metadata edits, run and report profile
scans and candidate files. Required version changes synchronize the canonical
source and profile-required mirrors; independent example application versions
change only with explicit request. Report disagreements before editing and block
ambiguous targets. Afterward report canonical value, synchronized files,
intentional non-changes, scans, validation, and mismatches. A required bump
missing at checkpoint or integration blocks readiness unless the user explicitly
declines it for the current action. Governance/documentation-only work skips the
bump with a reason.