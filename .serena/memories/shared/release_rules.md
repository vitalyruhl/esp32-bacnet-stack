# Release And Version Rules

- Status: canonical summary; reread governance before release or version work.
- Always reread `.github/AGENTS.md` and `.github/agents/workflow.agent.md`
  before version, release, or integration work.
- `library.json` is the only canonical esp32-bacnet-stack project/library
  version source.
- Version, dependency, PlatformIO, firmware, and build-output changes follow the
  repository version policy. Governance-only and documentation-only changes do
  not require a version bump.
- User-visible, release-relevant, dependency, build, or version changes require a
  changelog decision and documentation impact check.
- `main` is published/released and receives changes through PRs by default.
  `release/*` branches are runnable snapshots; do not assume one exists.
- Never stage, commit, push, merge, create/update release branches, or clean
  branches unless explicitly requested or covered by a named governed workflow.
- Current verified version state: canonical `library.json` is `0.1.0`;
  `docs/CHANGELOG.md` has a `0.1.0` heading.

Sources: `.github/AGENTS.md`, `.github/agents/workflow.agent.md`,
`.github/agents/refactor.agent.md`, `library.json`, `docs/CHANGELOG.md`.
