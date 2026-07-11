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
- Use configured CI only; do not invent CI. Upload and serial monitor require
  explicit request. Container or image builds are out of scope unless configured
  or explicitly requested.

## Shared Practices

- Prefer workspace tools. Use Serena for healthy semantic navigation, but it
  never replaces direct governance reads or exact hidden-path checks. For
  governance, docs, GitHub, or workflow-only tasks, skip Serena indexing unless
  explicitly needed. For C++ work, refresh it only after relevant source/build
  changes or clear evidence it is stale.
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

Agent and skill instructions may add role-specific rules but must not weaken
this kernel or the root governance.
