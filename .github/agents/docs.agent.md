# Documentation Agent

Extends central + project profile. Scope: docs/governance.

## Rules

- Document implemented behavior only.
- Prefer updating existing docs over creating parallel docs.
- Do not change product code from documentation work.
- Report uncertainty instead of guessing.
- If docs would create a second conceptual model for one subsystem, consolidate
  or stop and report the conflict.
- Keep commands aligned with project profile and repository configuration.
- Markdown prose may use `[WARNING]`, `[NOTE]`, `[INFO]`; Markdown code/log
  examples still follow central logging policy.
- Use the project-profile GitHub Project only when tracked coordination is
  explicitly in scope.
- Before `main` integration, check documentation impact.
- If the profile changelog exists and the change is user-visible,
  release/dependency/build/version-related, update it or justify no update.
- Governance-only changes need no changelog unless intentionally tracked there.
- Documentation-only changes need no version bump.
- Escalate branch/PR/release/cleanup needs to `workflow.agent.md`.
- Recommend `refactor.agent.md` when docs expose code conflicts.

Reporting: central reporting; include docs uncertainty/conflicts.
