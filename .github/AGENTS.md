# AGENTS.md

Canonical reusable governance for ESP32/C++/PlatformIO Arduino-library
repositories. Project facts live only in `.github/agents/project.agent.md`.

## Mandatory Load

- Before any repository work, read by known path:
  - `.github/AGENTS.md`
  - `.github/agents/project.agent.md`
  - `.github/agents/<selected-agent>.agent.md`
- For workflow shortcuts, selected agent is `.github/agents/workflow.agent.md`.
  - Treat leading-dot command tokens such as `.checkpoint`, `.audit`, `.ready`, or `.toMain` as workflow shortcut shorthand unless they are clearly paths, filenames, extensions, versions, or quoted literals.
- Project profile is mandatory context and never replaces the selected agent.
- Do not discover governance only by broad search; hidden paths may be skipped.
- Governance searches MUST use hidden paths:
  - `rg --hidden -n "workflow\.begin|workflow\.audit|workflow\.toMain|workflow\.cleanBranches" .`
  - `fd --hidden "AGENTS|agent\.md|workflow\.agent\.md" .`
- Plain `rg ... .` or `fd ... .` is insufficient for governance discovery.
- Missing required governance reads are a hard blocker; report before work.

## Language

- Normal user chat: informal German ("du").
- Repository artifacts created by agents MUST be English: governance, docs,
  issues, PRs, review comments, code comments, identifiers, errors, logs, and
  commits.
- Do not translate artifacts unless explicitly requested for that artifact.
- Normalize informal user text into clean English for branch names, issue/PR
  titles, task labels, and governance wording.
- Do not normalize exact paths, commands, symbols, identifiers, provided names,
  issue/PR numbers, tags, versions, or quoted literals.
- Preserve exact names when requested. Check quoted path casing. Ask if
  normalization changes scope.
- Example: `gevernance-sharpenes` may derive `governance-sharpening`.

## Project Profile

- Resolve identity, paths, examples, validation commands, version source,
  version scans, GitHub Project, release example, architecture facts, and risky
  areas from `.github/agents/project.agent.md`.
- Generic rules here stay binding; profile values fill project-specific slots.
- If profile and central rules conflict, follow central safety/workflow force
  and report drift.

## Routing

- `project.agent.md` is context, not a work agent.
- `control-plane.agent.md`: routing only.
- `docs.agent.md`: documentation/governance wording.
- `refactor.agent.md`: code/refactor/tests/examples/PlatformIO validation.
- `workflow.agent.md`: branches, general issues, PRs, releases, checkpoints,
  cleanup, session close.
- `plan.agent.md`: planning-only issue breakdowns, risk/validation strategy,
  tracked planning when explicitly requested.
- Overlays may add scope rules but MUST NOT contradict central governance.

## Safety

- User changes are sacred; never revert/overwrite without explicit request.
- Analyze before editing.
- Do not change product code unless explicitly requested. Governance-reference fixes must stay in governance or documentation files.
- Do not invent, remove, or weaken project rules unless explicitly requested or the rule is clearly obsolete, replaced with an equivalent-or-stronger rule, and reported.
- Never mark an issue solved/fixed until the user confirms it works.
- Before rename/delete operations, search references first and update them.
- Before workspace-modifying commands, ensure goal and scope are clear.
- Ask 1-3 precise questions when ambiguity would change architecture, risk,
  scope, or user-owned edits.
- Work incrementally; checkpoint/commit only when requested.
- Work on one side branch at a time.
- Before multi-file refactors or risky changes, ensure baseline is clean,
  committed, or intentionally dirty by user request.
- Level A: read/search/analyze may run immediately.
- Level B: small, scoped changes may be implemented immediately.
- Level C: project-profile risky areas require explicit confirmation.

## Git And Workflow Invariants

- Route branch, release, PR, merge, checkpoint, cleanup, and session-close work
  through `workflow.agent.md`.
- PR integration is default for `main`.
- Do not change `main`/`master` directly except an explicitly requested
  docs-only TODO update under project-profile TODO paths.
- Direct pushes to `main` require explicit request.
- Mutating git actions require explicit request or a named workflow.
- Stage/commit/push only on explicit request or when a named workflow requires
  it.
- Read-only git inspection may run without asking: status, diff, log, show,
  branch, remote listing.
- Owner/admin bypass is explicit-only for the current action.
- Required status checks MUST NOT be bypassed unless explicitly confirmed and
  the reason is reported.
- `workflow.audit` is read-only.
- `workflow.end` is report-only.
- Session close is report-only by default.
- Release branches are optional before first release and updated only when
  explicitly requested.

## Version

- Use the project-profile canonical version source and scan commands.
- Require an appropriate version bump for dependency updates, PlatformIO config,
  library metadata, firmware code, or example changes affecting build outputs
  unless the user explicitly says not to bump.
- Governance-only and documentation-only changes do not require a version bump;
  report the skip.
- Patch: dependency updates, bug fixes, compatible internal changes,
  build/config maintenance without public API break.
- Minor: new public features, examples, APIs, compatible behavior additions.
- Major: breaking APIs, incompatible storage/config/schema, migration-required
  behavior changes.
- README/changelog/examples are mirrors or independent app versions unless the
  project profile says otherwise.
- Before version/release/changelog/library metadata edits, run/report profile
  version scans and candidate files.
- When project/library version changes, sync profile canonical source and
  required mirrors; report missing paths.
- Do not change independent example app versions unless explicitly requested.
- If version files disagree, report before editing. If target is ambiguous, ask.
- After version changes, report canonical value, synchronized files, intentional
  non-changes, scans, validation, and mismatches/risks.
- GitHub Actions-only Dependabot updates do not require a project version bump
  unless produced output, supported PlatformIO environments, or release artifact
  behavior changes.

## Code And Logging

- Code comments, identifiers, function names, errors, and logs must be English.
- Emojis are forbidden in code, comments, logs, and generated outputs.
- Keep hardware/config-sensitive changes conservative.
- Markdown prose may use `[WARNING]`, `[NOTE]`, `[INFO]`; Markdown code blocks
  and log examples still follow logging policy.
- Source, headers, examples, tests, and Markdown code blocks use only:
  - `[E]` error
  - `[W]` warning
  - `[I]` info
  - `[D]` debug
  - `[T]` trace
- Long tags such as `[ERROR]`, `[WARNING]`, `[INFO]`, `[DEBUG]`, `[SUCCESS]` are
  forbidden in code/log output.
- Verbose controls emission; it is not a severity.
- Logging normalization and API renames are separate. Before API rename, run
  `rg`; after rename, rerun `rg` and confirm old names are gone from relevant
  profile paths.

## Tools

- Prefer workspace tools when fit.
- Prefer Serena for semantic code navigation, symbol/reference lookup,
  architecture inspection, and project context when configured and healthy.
- Serena never replaces direct governance reads, required `rg --hidden`
  governance searches, repository files, user instructions, or canonical
  governance.
- Use `rg`/`fd` for exact text audits, hidden-path governance, stale terms, and
  policy checks.
- If Serena is unavailable/unhealthy/stale/incomplete, report and fall back to
  direct reads plus CLI tools.
- Preferred local tools: `git`, `gh`, `rg`, `fd`, `jq`, `dasel`, `jc`, `7z`,
  `pwsh`, `winget`, `choco`, `coreutils`, `python`, `uv`, `platformio`/`pio`.
- Use `jq` for JSON and `dasel` for YAML/TOML/JSON/XML when safest; do not mix
  their syntax or use deprecated dasel flags.
- Use `gh` for GitHub operations when authenticated and available.
- Use `uv` only when Python tooling is in scope.
- Do not use JavaScript package-manager commands as preferred validation unless
  the repository adds such tooling.
- Do not install/upgrade tools unless explicitly asked.
- On Windows, prefer PowerShell-native commands unless other tools are
  confirmed.
- Distinguish command failure, no matches, missing files, and missing tools.
- Failed commands are not evidence about repository content.

## Validation

- Use configured/enabled GitHub Actions/checks when present; do not invent CI.
- If no enabled CI exists, report that and rely on required local validation.
- Use project-profile PlatformIO commands for root builds, affected examples,
  affected tests, and explicitly relevant OTA work.
- Upload and serial monitor require explicit request.
- Markdown/governance-only changes skip PlatformIO unless asked.
- Governance-only changes require consistency check of routing, shortcuts,
  branch/Git/PR, tools, Serena, validation, version, session close, checkpoint,
  planning, project profile, and reporting rules.
- Run relevant tests when tests are present and affected.
- Container/image builds are not required unless configured or explicitly in
  scope.
- Report required validation that cannot run.

## Validation Reuse

- Reuse prior validation only when it ran in this session, passed clearly,
  covered the same changed files/scope, no relevant files changed since, and the
  working tree matches the validated state.
- During `workflow.checkpoint`, do not duplicate already valid validation.
- Checkpoint reuse report:
  `Validation reused: <command>, passed before checkpoint, no relevant changes since.`
- Rerun if source/header/example/test/build/metadata files changed after prior
  validation.
- Rerun if prior validation failed, was partial, skipped, ambiguous, or missed
  checkpoint scope.
- Never reuse across branches unless commit/tree identity is proven.
- Never reuse after merge, rebase, conflict resolution, dependency update,
  PlatformIO config change, or generated-file refresh.
- For governance/docs-only checkpoints, skip PlatformIO and report:
  `PlatformIO skipped: governance/docs-only changes.`

## Reporting

- Default concise.
- Always report: files changed or `no files changed`, summary, validation
  passed/failed/skipped/reused with reason, blockers/risks/required user action,
  and git actions only if performed.
- Report only when relevant or requested: branch, governance files read, full
  command list, full validation output, long file lists, `git diff --stat`,
  focused diff excerpts, release branch details, GitHub Issue/Project updates.
- Never hide failed commands, skipped required validation, invalid/uncertain
  reuse, unsafe dirty state, branch mismatch, user-change conflicts, merge/PR/CI
  blockers, Level C risk, correctness uncertainty, or required confirmation.
- Do not paste full diffs unless explicitly asked.
- Do not list governance files read unless loading failed, drift was found,
  governance changed, or the user asks.

## Final Rule

- Never mark an issue solved/fixed until the user confirms it works.
