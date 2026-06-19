# Release And Version Rules

- Status: release/version orientation summary only.
- This memory is not canonical governance.
- Reread `.github/AGENTS.md`, `.github/agents/project.agent.md`, and `.github/agents/workflow.agent.md` before version, release, or integration work.

## Version Source

- `library.json` is the canonical project/library version source.
- Current verified version state at the time this memory was written:
  - `library.json`: `0.1.0`
  - `docs/CHANGELOG.md`: contains a `0.1.0` heading
- Verify the current version from repository files before version or release work.

## Version Bump Reminder

- Governance-only and documentation-only changes do not require a version bump.
- GitHub Actions-only Dependabot updates do not require a project version bump unless produced output, supported PlatformIO environments, or release artifact behavior changes.
- Dependency updates, PlatformIO config changes, library metadata changes, firmware code changes, or example changes that affect build output require a version decision.
- New compatible public features or APIs usually require a minor version bump.
- Breaking APIs, incompatible storage/config/schema, or migration-required behavior require a major version bump.

## Release / Integration Reminder

- `main` is the default integration branch and should receive changes through PRs by default.
- `release/*` branches are optional before the first release and must not be created or updated unless explicitly requested.
- Never stage, commit, push, merge, create/update release branches, or clean branches unless explicitly requested or covered by a named governed workflow.
- User-visible, release-relevant, dependency, build, or version changes require a changelog/documentation impact decision.

Sources: `.github/AGENTS.md`, `.github/agents/project.agent.md`, `.github/agents/workflow.agent.md`, `library.json`, `docs/CHANGELOG.md`.