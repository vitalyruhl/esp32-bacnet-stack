# AGENTS.md

Canonical governance for `esp32-bacnet-stack`, a C++ ESP32 BACnet/IP stack
library built with PlatformIO and the Arduino framework.

## Mandatory Governance Load

- Before any agent command, workflow shortcut, branch action, validation, file
  edit, commit, push, pull request, merge, release update, cleanup, or GitHub
  tracking action, read:
  - `.github/AGENTS.md`
  - `.github/agents/<selected-agent>.agent.md`
- For workflow shortcuts, the selected agent is
  `.github/agents/workflow.agent.md`.
- Read known governance paths directly. Do not rely on repository-wide search
  to discover governance because `.github/` may be hidden.
- Governance searches MUST include hidden paths:
  - `rg --hidden -n "workflow\.begin|workflow\.audit|workflow\.toMain|workflow\.cleanBranches" .`
  - `fd --hidden "AGENTS|agent\.md|workflow\.agent\.md" .`
- Plain `rg ... .` or `fd ... .` is not sufficient for governance discovery.
- Failure to read required governance files is a hard blocker; report it before
  further action.

## Language

- Use informal German ("du") in normal user chat.
- Keep user summaries brief unless detail is requested.
- Repository artifacts created by agents MUST be English: governance, docs,
  issues, PRs, review comments, code comments, identifiers, errors, logs, and
  commit messages.
- Do not translate repository artifacts to German unless explicitly requested
  for that artifact.
- Normalize informal user text into clean English for branch names, issue/PR
  titles, task labels, and governance wording.
- Do not normalize exact paths, commands, symbols, identifiers, provided names,
  issue/PR numbers, tags, versions, or quoted literals.
- Preserve names the user says to use exactly. Check quoted path casing against
  repository state. Ask if normalization changes scope.
- Example: `gevernance-sharpenes` may derive `governance-sharpening`.

## Repository Scope

- Primary configuration: `platformio.ini`.
- Canonical library metadata and version source: `library.json`.
- Main library code: `src/`.
- Public headers: `include/` when present.
- Internal libraries: `lib/` when present.
- Examples: `examples/`, including `client-demo` and `server-demo`.
- Documentation: `README.md` and `docs/`.
- Tests: `test/` when present.
- Tooling: `tools/`.
- Serena metadata: `.serena/`; memories are summaries only and never override
  repository files, user instructions, or canonical governance.
- Wokwi files may live under example-specific `Wokwi/` folders.
- Do not assume unrelated application, backend, or release flows unless the
  repository adds them explicitly.

## Architecture

- Core library roles are `BacnetClient` and `BacnetServer`.
- Keep reusable BACnet stack code separate from examples, tests, generated
  artifacts, and repository tooling.
- Keep protocol encoding and decoding boundaries explicit.
- Treat BACnet protocol semantics, network-facing behavior, storage/NVS, OTA,
  security, build pipelines, and large refactors as Level C/risky.
- Do not add core dependencies on optional demo integrations.
- Do not import external BACnet implementation code without explicit license,
  provenance, and architecture review.

## Agent Routing

- `.github/AGENTS.md` is canonical.
- `.github/agents/control-plane.agent.md` routes only.
- Use `.github/agents/docs.agent.md` for documentation and governance wording.
- Use `.github/agents/refactor.agent.md` for code changes, refactors, tests,
  examples, PlatformIO configuration, and build validation.
- Use `.github/agents/workflow.agent.md` for branches, issues, PRs, releases,
  checkpoints, cleanup, and session-close workflows.
- Use `.github/agents/plan.agent.md` for high-reasoning planning, risk
  analysis, validation strategy, issue breakdowns, and tracked planning when
  explicitly requested.
- Agent overlays may add scope-specific rules but MUST NOT contradict this file.
  If they do, follow this file and report drift.

## Tracking

- GitHub Issues and PRs may be used for tracked work when useful.
- Current GitHub Project: `ESP32 BACnet Stack` (#6).
- Use the configured GitHub Project only when tracked workflow or project
  coordination is explicitly in scope.
- Do not invent project-board updates for untracked work.
- For larger tracked planning, prefer one parent issue plus smaller step
  issues, subissues when supported, or linked child issues with a parent
  checklist when subissues are unavailable.

## Safety And Autonomy

- User changes are sacred; never revert or overwrite them without explicit
  request.
- Analyze before editing.
- Do not make product-code changes unless explicitly requested or strictly
  required to correct governance references.
- Do not invent project rules or remove existing project-specific rules unless
  consciously replacing obsolete governance.
- Never mark an issue as solved or fixed until the user confirms it works.
- Before rename/delete operations, search references first and update them.
- Before workspace-modifying terminal commands, ensure goal and scope are clear.
- Ask 1-3 precise questions or propose 2-3 options when requirements are
  ambiguous or the change impacts multiple subsystems.
- Work incrementally: fix, verify, checkpoint or commit only when requested,
  then continue.
- Work on one side branch at a time.
- Before multi-file refactors or risky changes, ensure the baseline is
  understood and clean, committed, or intentionally dirty by user request.
- Level A read-only search, read, and analysis may run immediately.
- Level B small, clearly scoped changes may be implemented immediately.
- Level C risky areas require explicit confirmation.

## Workflow And Git

- Route branch, release, PR, merge, checkpoint, cleanup, and session-close work
  through `workflow.agent.md`.
- Integrate into `main` through pull requests by default.
- `main` is the published/released branch.
- `release/*` branches are runnable snapshots and must stay buildable and
  runnable.
- `release/*` branches are versioned by release, for example `release/v0.1.0`.
- Before the first release, `release/*` branches are optional.
- Do not assume `release/*` exists. Missing release branches do not block normal
  PR-based `main` integration unless release sync is explicitly in scope.
- Create or update release branches only when explicitly requested.
- `feature/*` branches are work-in-progress and may be unfinished or temporarily
  broken.
- Do not change `main` directly.
- If active branch is `main` or `master` and file-changing work is requested,
  stop before editing and use workflow rules to create or select a side branch.
- Only exception: an explicitly requested docs-only TODO update under
  `docs/TODO.md` or `docs/todo_*.md` may be committed directly to `main`.
- Direct pushes to `main` are forbidden unless explicitly requested.
- Fast-forward integration to `main` is allowed only when explicitly requested
  as fast-forward or `ff`.
- Read-only git commands may run without asking: `git status`, `git diff`,
  `git log`, `git show`, `git branch`, `git remote -v`.
- Mutating git commands require explicit confirmation: `git add`, `git commit`,
  `git switch`/`git checkout`, `git reset`, `git merge`, `git rebase`,
  `git clean`, `git stash`, `git cherry-pick`.
- Stage, commit, and push only on explicit user request or when a named workflow
  explicitly requires it.
- If staging is requested, prefer `git add -A`.
- Do not prepend `Set-Location` to git commands. Use the configured workdir.
  For non-git directory changes, use `Push-Location`/`Pop-Location`.

## Pull Requests And Protection

- Prefer `gh` for PRs, CI checks, issues, and project operations when
  authenticated and available.
- Keep PRs scoped to one coherent change.
- Before preparing or merging into `main`, run or report validation and perform
  a documentation impact check.
- If docs are affected, route through `docs.agent.md` before integration.
- If `docs/CHANGELOG.md` exists and the change is user-visible,
  release-relevant, dependency-related, build-related, or version-related,
  update it or justify why no changelog update is needed.
- Governance-only changes do not require changelog entries unless the repository
  intentionally tracks governance changes there.
- Documentation-only changes do not require a version bump.
- Maintainer-owned PRs do not require external review by governance and may
  merge when required validation passed and branch protection allows.
- External contributor PRs require review before merge.
- Owner/admin bypass is explicit-only for the current action.
- Required status checks MUST NOT be bypassed unless the user explicitly
  confirms that exception and the reason is reported.
- If branch protection blocks merge, report the blocker and do not bypass it.
- If maintainer-owned PRs are blocked only by review requirements after
  validation passed, report that bypass may be possible when configured and
  recommend adjusting branch protection if review is not intended.

## Workflow Shortcuts

- Shortcuts run sequentially. If `workflow.audit` blockers remain, stop before
  merge, cleanup, release updates, or destructive actions.
- Follow-up shortcuts after `workflow.audit` run only when explicitly requested
  and no blockers remain.
- `workflow.begin`: create/select the proper branch, derive a clean English
  branch name from task intent, report it, do not edit code or build files, and
  do not run implementation work unless explicitly requested after branch setup.
- `workflow.checkpoint`: verify branch/status, refuse direct `main`/`master`
  except the docs-only TODO exception, inspect `git diff --stat`, stage with
  `git add -A`, create one meaningful English commit, push only the current work
  branch, and run or report relevant validation.
- `workflow.docs`: perform narrow documentation-only synchronization.
- `workflow.audit`: read-only only; no file changes, branch changes, commits,
  merges, release updates, cleanup, or destructive actions; report blockers
  before any follow-up shortcut.
- `workflow.ship`: build and verify artifacts without implicit merge.
- `workflow.ready`: prepare review/integration, run or report validation, do not
  merge to `main`, do not update `release/*`, and do not push unless explicitly
  requested or covered by a named workflow.
- `workflow.toMain`: use PR workflow by default unless fast-forward/`ff` is
  explicitly requested; commit, push, PR creation, merge, and cleanup are
  allowed only as part of this explicit workflow; run/report validation; perform
  documentation impact check; report required reviews, failing checks, conflicts,
  and branch protection; use bypass only when explicitly requested; never bypass
  required checks without explicit confirmed exception and reported reason.
- `workflow.cleanBranches`: delete only branches verified as integrated; skip
  active, unmerged, or ambiguous branches and report reasons.
- `workflow.end`: report current branch, changed files, validation state, and
  blockers only; do not commit, push, merge, switch branches, update release
  branches, or claim merge/fix success.
- Session close is report-only by default. Do not commit, push, merge, switch
  branches, or update release branches during session close unless the user
  explicitly invokes `workflow.checkpoint`, `workflow.toMain`, or an explicit
  release-sync workflow.

## Version Policy

- Require an appropriate project version bump for dependency updates,
  PlatformIO configuration changes, library metadata changes, firmware code
  changes, or example changes that affect build outputs unless the user
  explicitly says not to bump.
- Governance-only and documentation-only changes do not require a version bump;
  report that it was skipped by policy.
- Use patch for dependency updates, bug fixes, compatible internal changes, and
  build/configuration maintenance with no public API break.
- Use minor for new public features, examples, APIs, and compatible behavior
  additions.
- Use major for breaking public APIs, incompatible storage/NVS layout,
  incompatible configuration schema, or behavior requiring migration.
- `library.json` is the canonical esp32-bacnet-stack project/library version.
  README text, changelog text, and examples are mirrors or independent app
  versions, not sources of truth.
- Before changing versions, release metadata, changelog/release notes, library
  metadata, or files that may contain the project version, scan and report
  candidate files:
  - `library.json`
  - `README.md`
  - `docs/CHANGELOG.md` or resolved changelog path
  - README version badges or mentions
  - additional files found by ripgrep containing current or target version
- Version scan commands:
  - `rg -n "version|VERSION|ESP_BACNET_VERSION|BACNET_STACK_VERSION" .`
  - `rg -n "<current-version>|<target-version>" .`
- When project/library version changes, synchronize `library.json`, README
  version badges/mentions, changelog headings/mentions, and examples that
  intentionally carry the library/app version. Report missing required paths.
- Keep client and server example version references aligned with the
  esp32-bacnet-stack project/library version if such references are added.
- Do not change other example firmware/app versions unless explicitly requested.
- If a package-managed demo is added later, define whether its metadata mirrors
  `library.json` or has an independent version before editing it.
- Version changes affecting published library output require a changelog update
  unless the governing issue explicitly says otherwise.
- If version files disagree before work starts, report the mismatch. If target
  version is ambiguous, stop and ask.
- After version changes, report canonical `library.json` version, synchronized
  files and versions, files intentionally unchanged, scan commands, validation,
  and remaining mismatch or risk.
- GitHub Actions-only Dependabot updates do not require a `library.json` bump
  unless they change produced library output, firmware build output, supported
  PlatformIO environments, or release artifact behavior.

## Code And Logging

- Code comments, identifiers, function names, errors, and logs must be English.
- Emojis are forbidden in code, comments, logs, and generated outputs.
- Keep hardware-facing and configuration-sensitive changes conservative.
- Markdown prose may use readable long-form tags such as `[WARNING]`, `[NOTE]`,
  and `[INFO]`; code blocks and log examples inside Markdown are not exempt from
  logging policy.
- Source code, headers, examples, tests, and Markdown code blocks must use
  short ASCII severity tags only:
  - `[E]` error
  - `[W]` warning
  - `[I]` info
  - `[D]` debug
  - `[T]` trace
- Long tags such as `[ERROR]`, `[WARNING]`, `[INFO]`, `[DEBUG]`, and `[SUCCESS]`
  are forbidden in code/log output.
- Verbose controls emission; it is not a severity level.
- Severity reassignment is allowed when technically justified.
- Logging normalization and API renames are separate; do not mix them.
- Before API rename, search references with `rg`; after rename, rerun `rg` and
  confirm old names do not remain in `src/`, `include/`, `lib/`, `test/`,
  `docs/`, or examples when present.

## Tool Policy

- Prefer available agent-workspace tools when fit.
- Prefer Serena when configured, healthy, and useful for semantic C++ navigation,
  symbol lookup, references, architecture inspection, and project-context review.
- Serena does not replace mandatory direct governance reads, required
  `rg --hidden` governance/policy searches, repository files, user
  instructions, or canonical governance.
- Use direct `rg`/`fd` for plain text audits, exact checks, generated-file
  scans, stale-term searches, and hidden-path policy checks.
- If Serena is unavailable, unhealthy, stale, incomplete, or missing relevant
  memories, report it and fall back to `rg`, `fd`, `git`, and direct reads.
- Preferred local tools: `git`, `gh`, `rg`, `fd`, `jq`, `dasel`, `jc`, `7z`,
  `pwsh`, `winget`, `choco`, `coreutils`, `python`, `uv`, `platformio`/`pio`
  when installed.
- Use `rg --hidden` for governance, agent instructions, workflow shortcuts,
  branch rules, PR rules, issue rules, validation rules, version rules, and
  repository policy searches.
- Use `fd --hidden` for governance discovery.
- If `rg` or `fd` is missing for required audits, report it and give a simple
  install hint instead of silently using a weaker search path.
- Use `jq` for JSON and `dasel` for YAML, TOML, JSON, or XML when safest. Do not
  mix their syntax or use deprecated dasel flags.
- Use `gh` for GitHub operations when authenticated and available.
- Use `uv` only when Python tooling is actually in scope.
- Do not use JavaScript package-manager commands as preferred validation tools
  unless the repository adds such tooling.
- Do not install or upgrade tools unless explicitly asked.
- On Windows, prefer PowerShell-native commands unless other tool availability
  is confirmed.
- Distinguish command failure, no matches, missing files, and missing tools in
  reports.
- Do not treat failed commands as evidence about repository content.

## Validation

- Use configured and enabled GitHub Actions/checks when present; do not invent
  required CI.
- If no enabled CI is configured, report that and rely on required local
  validation.
- After `.cpp` or `.h` changes, run at least:
  - `pio run -e usb`
- For affected client example changes:
  - `pio run -d examples/client-demo -e usb`
- For affected server example changes:
  - `pio run -d examples/server-demo -e usb`
- For affected tests:
  - `pio test -e usb --without-uploading --without-testing`
- For OTA-specific behavior or configuration when explicitly relevant:
  - `pio run -e ota`
- Upload and serial monitor commands require explicit user request.
- If only Markdown or governance files changed, skip PlatformIO unless asked.
- Governance-only changes require a consistency check: read affected governance
  files and verify routing, shortcut, branch, Git, PR, tool, Serena, validation,
  version, session-close, checkpoint, planning-agent, and reporting rules do not
  contradict each other.
- Run relevant tests when tests are present and affected.
- Docker or image builds are not required unless configured here or explicitly
  in scope.
- If required validation cannot run, report it plainly.

## Mandatory Reporting

After file-changing work, report:

- branch used
- governance files read when governance, workflow, branch, PR, merge, release,
  validation, version, or cleanup rules were in scope
- files changed
- concise summary
- validation run
- skipped validation with reason
- git actions performed
- remaining risks or blockers

Inspect relevant diffs before reporting. Do not paste full diffs unless asked;
prefer `git diff --stat`, changed file lists, and focused summaries.

## Final Rule

- Never mark an issue as solved or fixed until the user explicitly confirms it
  works.
