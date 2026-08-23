# Shared Governance Kernel

This is the compact routing and policy index. Repository facts belong in the
project profile; detailed repeatable procedures belong in on-demand skills.
The root `AGENTS.md` owns universal safety, language, and loading rules.

## Loading And Routing

- Use one minimal governance gate at task start, then load the selected role
  and only the required skills. Reuse stable context in the same session.
- Reload after an actual role change, relevant governance change, uncertainty,
  or an explicit fresh audit. Never reload unrelated agents merely because
  another governance file changed.
- `.github/agents/project.agent.md` is context, not a work agent. Load it only
  for project paths, validation commands, version policy, architecture facts,
  or Level C classification.
- `control-plane` routes work. `docs` owns documentation and governance edits.
  `plan` is read-only planning. `refactor` owns product implementation.
  `audit` and `architecture-audit` are read-only reviews. `workflow` owns Git,
  GitHub, release, and final-workflow decisions.
- Treat `workflow.begin`, `.begin`, `.beginn`, `workflow.checkpoint`,
  `.checkpoint`, `workflow.docs`, `workflow.audit`, `.audit`, `workflow.ship`,
  `workflow.ready`, `.ready`, `workflow.toMain`, `.toMain`,
  `workflow.toServer`, `.toServer`,
  `workflow.cleanBranches`, `.cleanBranches`, and `workflow.end` as workflow
  shortcuts unless clearly quoted literals or paths.
- Use `rg --hidden` or an equivalent hidden-path search for governance audits;
  ordinary non-hidden discovery is insufficient.

## Scope And Safety

- Analyze before editing. Keep product changes explicitly requested and
  governance-reference fixes within governance or documentation files.
- Before a rename or delete, search references and update them. Before a
  workspace-modifying command, confirm its goal and scope.
- Level A read/search/analysis may proceed. Level B small scoped changes may
  proceed. Level C areas named by the project profile need explicit user
  confirmation.
- Keep one side branch active. Before multi-file or risky work, ensure the
  baseline is clean, committed, or intentionally dirty by user request.
- `main` and `server` are permanent integration branches and must never be
  deleted or renamed by cleanup or final synchronization. Client, shared,
  repository, and general work normally integrates into `main`; server-specific
  work normally integrates into `server`. Integrate `server` into `main` only
  through an explicit release/integration request.
- Use configured CI only; do not invent CI. Upload and serial monitor require
  explicit request. Container or image builds are out of scope unless configured
  or explicitly requested.

## Shared Practices

- Read governance directly before tool-assisted repository discovery. When
  ProjectAtlas is available, it is the primary orientation layer for task
  startup, folder/file
  ranking, compact summaries, indexed search, exact slices, broad static
  architecture candidates, purpose curation, health, and lint. Call one compact
  `atlas_session_brief`, follow its typed next call, and do not repeat discovery
  with another tool.
- When ProjectAtlas is unavailable, fall back to direct `rg`/`fd` and bounded
  source reads while preserving the same Serena semantic boundary.
- Use Serena after ProjectAtlas has narrowed the relevant file or symbol when
  LSP semantics are material: declarations, implementations, overloads,
  references, and diagnostics. Do not reconfirm trusted Atlas relations with
  Serena unless Atlas reports incomplete, ambiguous, unresolved, or truncated
  evidence that matters to the decision. Serena remains read-only unless a
  separate governance decision enables semantic writes.
- Neither tool replaces direct governance reads, current source evidence, or
  exact `rg --hidden` audits. For governance, docs, GitHub, or workflow-only
  tasks, skip Serena indexing unless explicitly needed. Refresh Atlas with
  `atlas_watch_once` after relevant changes; refresh Serena only under
  `serena-index-freshness`, and never reuse either index across branches without
  proven identical tree state.
- For plan, refactor, audit, and architecture-audit tasks that actually use
  Serena, load `serena-index-freshness`.
- Prefer `rg`/`fd` for exact audits, `gh` for authenticated GitHub operations,
  `jq` for JSON, and `dasel` for structured YAML/TOML/JSON/XML when safe. On
  Windows, prefer PowerShell-native commands. Do not install or upgrade tools
  unless requested. A failed command is not evidence of absent content.
- Code comments, identifiers, errors, and logs are English and contain no
  emojis. In source, headers, examples, tests, and Markdown code/log blocks,
  use only `[E]`, `[W]`, `[I]`, `[D]`, or `[T]`; verbose is not a severity.
  Markdown prose may use `[WARNING]`, `[NOTE]`, and `[INFO]`.
- Report concisely: changed files or `no files changed`, result, validation
  state, blockers/risks, and Git actions only when performed. Never hide failed
  commands, skipped required validation, unsafe dirty state, invalid reuse,
  required confirmation, or branch/CI/merge blockers.

## On-Demand Skills

- `validation-gate`: validation ordering, autofix handling, and valid reuse.
- `version-impact`: classification, bump policy, scans, and synchronization.
- `issue-project-sync`: explicit Issue/Project updates and read-back.
- `safe-branch-cleanup`: protected branch preservation and cleanup checks.
- `final-repository-sync`: post-integration repository synchronization.
- `docs-gate`: semantic documentation consistency review.
- `projectatlas` (installed plugin): Atlas-first repository orientation,
  incremental freshness, purpose curation, health, and lint.
- `serena-index-freshness`: precise Serena index refresh and reuse criteria.

Agent and skill instructions may add role-specific rules but must not weaken
this kernel or the root governance.
