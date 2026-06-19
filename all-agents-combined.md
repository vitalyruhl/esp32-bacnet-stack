## AGENTS.md

# Root Agent Instructions

This root file exists for tools that load repository-root `AGENTS.md` before
repository-specific governance.

## Canonical Governance

- `.github/AGENTS.md` is the canonical repository governance file.
- Before repository work, read `.github/AGENTS.md` and the applicable
  `.github/agents/*.agent.md` file directly.
- Do not rely on repository-wide search to discover governance files; read known
  governance paths directly.
- If this file conflicts with `.github/AGENTS.md`, follow `.github/AGENTS.md`.
- Defer to `.github/AGENTS.md` for repository workflow, branch, pull request,
  merge, release, validation, version, tool, and agent-routing rules.

## Communication

- Use informal German ("du") in normal chat with the user.
- Keep user-facing summaries brief unless detailed explanation is requested.
- Repository artifacts, code comments, commit messages, issue text, pull request
  text, and generated governance text follow the language rules in
  `.github/AGENTS.md`.

## Safety Baseline

- User changes are sacred. Never revert or overwrite user edits without explicit
  request.
- Do not stage, commit, push, merge, rebase, reset, clean, stash, switch
  branches, or update release branches unless the user explicitly requests it or
  a named workflow in `.github/AGENTS.md` requires it.
- Do not work directly on `main` or `master` except for the narrow exceptions
  defined in `.github/AGENTS.md`.
- Do not run upload, monitor, destructive, or hardware/network-affecting
  commands unless explicitly requested.

## Final Rule

- Never mark an issue as solved or fixed until the user explicitly confirms it
  works.

---

## .github/AGENTS.md

# AGENTS.md

Repository guidance for contributors and coding agents.

This repository is `esp32-bacnet-stack`, a C++/ESP32 BACnet/IP stack library
project built with PlatformIO and the Arduino framework. The repository targets
Arduino/PlatformIO library use with BACnet client and server roles.

## Mandatory Governance Load Preflight

Before executing any agent command, workflow shortcut, branch action,
validation, file edit, commit, push, pull request, merge, release-branch update,
or cleanup action, the agent MUST explicitly read the applicable governance
files.

Required reads:

- `.github/AGENTS.md`
- `.github/agents/<selected-agent>.agent.md`

For workflow shortcuts, the selected agent is:

- `.github/agents/workflow.agent.md`

The agent MUST NOT rely on repository-wide search to discover these files,
because hidden directories such as `.github/` may be skipped by default search
tools.

If governance files are known by path, read them directly instead of discovering
them through search.

If using ripgrep to inspect governance, the agent MUST include hidden paths, for
example:

rg --hidden -n "workflow\.begin|workflow\.audit|workflow\.toMain|workflow\.cleanBranches" .

If using fd to inspect governance, the agent MUST include hidden paths, for
example:

fd --hidden "AGENTS|agent\.md|workflow\.agent\.md" .

Plain `rg ... .` or plain `fd ... .` MUST NOT be treated as sufficient for
governance discovery, because they may skip hidden directories such as
`.github/`.

Failure to read the required governance files is a hard blocker and must be
reported before any further action.

## Communication

- Use informal German ("du") when talking to the user.
- Keep explanations short unless detail is explicitly requested.
- After completing work, summarize briefly.
- Internal chat with the user may remain informal German according to these
  communication rules.
- Only normal chat with the user may be German by default. Repository artifacts
  must remain in English unless the user explicitly requests a different
  language for a specific artifact.

## Repository Text Language

- Governance files under `.github/`, repository documentation, generated
  project text, issue text, pull request text, review comments created by
  agents, code comments, error messages, log messages, and commit messages
  created by agents must be written in English.
- Do not translate repository artifacts to German just because the user chat is
  German.
- If a specific artifact must be written in another language, require an
  explicit user request for that artifact.

## User Text Normalization

- User prompts may contain informal German, mixed German/English wording, and
  obvious spelling mistakes.
- Normalize free text into clean English when deriving branch names, issue/PR
  titles, task labels, or governance wording.
- Do not silently normalize exact paths, commands, symbols, identifiers,
  explicitly provided names, issue/PR numbers, tags, versions, or quoted
  literals.
- If the user says to use a name exactly, preserve it.
- Check quoted path casing against repository state.
- If intent is unclear or normalization changes scope, stop and ask.
- Example: `gevernance-sharpenes` may derive `governance-sharpening`.

## GitHub Text Language

- GitHub Issue titles must be written in English.
- GitHub Issue bodies must be written in English.
- GitHub Pull Request titles and descriptions must be written in English.
- GitHub comments created by agents should be written in English unless the user
  explicitly requests otherwise.
- Do not create German GitHub issue or PR text by default.
- These rules apply only to GitHub and repository artifacts, not to normal chat
  with the user.

## Repository Scope

- Primary project configuration: `platformio.ini`.
- Main firmware source code lives under `src/`.
- Examples live under `examples/`.
- Supporting documentation lives under `docs/` and `README.md`.
- Tests live under `test/` when present.
- Support scripts and repository tooling live under `tools/`.
- Optional demo integrations may live under `examples/` when present.
- Wokwi simulation files may live under example-specific `Wokwi/` folders.
- Do not assume Python application release flows or a standalone backend web
  service unless the repository explicitly introduces them.

## Agent Routing

- `.github/AGENTS.md` is the canonical source for repository-wide rules.
- `.github/agents/control-plane.agent.md` only routes work to the correct agent.
- Use `.github/agents/refactor.agent.md` for code changes, refactors, tests, and
  build validation.
- Use `.github/agents/workflow.agent.md` for branches, issues, PRs, releases,
  checkpoints, and explicit session-close workflows.
- Use `.github/agents/docs.agent.md` for documentation work.
- Agent files may add scope-specific rules, but they must not contradict this
  file. If they do, follow this file and report the drift.

## Tracking Policy

- GitHub Issues may be used for tracked work when useful.
- GitHub Pull Requests may be used for review and integration when useful.
- GitHub Project usage is optional and repository-specific.
- Current GitHub Project: `ESP32 BACnet Stack` (#6)
- When tracked workflow or project coordination is relevant, agents may use the
  configured GitHub Project.
- Do not invent mandatory project-board updates for tasks that do not actually
  use tracking.
- If the configured GitHub Project changes later, update this file and keep
  workflow guidance consistent with the new value.

## Safety Principles

- User changes are sacred. Never revert or overwrite user edits without asking.
- Analyze before modifying files.
- Do not make functional project-code changes unless explicitly requested or
  strictly required to correct governance references.
- Do not invent project rules. Preserve existing project-specific rules unless
  they are clearly obsolete or consciously replaced by newer governance.
- Do not mark an issue as solved or fixed until the user confirms it works.
- Before rename/delete operations, search references first and update them.
- Before terminal commands that modify the workspace, ensure goal and scope are
  clear.
- If requirements are ambiguous or a change impacts multiple subsystems/files,
  ask 1-3 precise questions or propose 2-3 options before editing.
- Work incrementally in small steps: fix, verify, checkpoint or commit only when
  requested, then continue.
- Work on one side branch at a time.
- Before multi-file refactors or other risky changes, ensure the baseline is
  understood and either clean, committed, or intentionally dirty by user
  request.

## Autonomy Levels

- Level A, safe: read-only actions such as search, read, and analysis may be done
  immediately.
- Level B, normal: small, clearly scoped changes may be implemented immediately.
- Level C, risky: changes involving BACnet protocol semantics, network-facing
  behavior, storage/NVS, OTA, security, build pipelines, or large refactors
  require explicit confirmation.

## Repository Workflow Rules

- Route branch, release, PR, merge, checkpoint, and cleanup operations through
  `.github/agents/workflow.agent.md`.
- Integrate into `main` through pull requests by default.
- `main` is the published or released branch.
- `release/*` branches are runnable snapshot branches and must stay buildable
  and runnable.
- `release/*` branches are versioned by release, for example `release/v4.0.0`.
- Do not assume `release/*` branches exist. Missing release branches do not
  block normal pull-request based `main` integration unless release sync is
  explicitly in scope.
- If release sync is explicitly requested and no suitable release branch exists,
  report that and skip the release update unless the user explicitly asks to
  create or update a release branch.
- `feature/*` branches are work-in-progress branches and may be unfinished or
  temporarily broken.
- Do not change `main` directly.
- Docs-only TODO updates under `docs/TODO.md` or `docs/todo_*.md` may be
  committed directly to `main` only when the user explicitly requests that
  workflow.
- If the active branch is `main` or `master` and file-changing work is
  requested, stop before editing and route through
  `.github/agents/workflow.agent.md` to create or select a proper side branch.
- The only direct-edit exception on `main` or `master` is an explicitly
  requested docs-only TODO update under `docs/TODO.md` or `docs/todo_*.md`.
- Direct pushes to `main` are forbidden unless the user explicitly requests an
  exception.
- Fast-forward integration to `main` is allowed only when the user explicitly
  requests fast-forward or `ff`.
- Workflow shortcuts may be chained only in sequence. `workflow.audit` remains
  strictly read-only and must finish by reporting blockers before any follow-up
  shortcut runs.
- If `workflow.audit` is followed by `workflow.toMain` or
  `workflow.cleanBranches`, continue only when the user explicitly requested the
  follow-up shortcut and no blockers remain.
- If blockers remain after audit, stop before merge, cleanup, release updates,
  or other destructive actions.
- Before preparing or merging changes into `main`, perform a documentation
  impact check.
- The documentation impact check must decide whether changed files or behavior
  require documentation updates.
- If documentation is affected, route the work through
  `.github/agents/docs.agent.md` before integration.
- If `docs/CHANGELOG.md` exists and the change is user-visible,
  release-relevant, dependency-related, build-related, or version-related,
  update it or explicitly justify why no changelog update is needed.
- If documentation is not affected, report that documentation sync is not
  required and why.
- Do not invent documentation updates for purely internal or governance-only
  changes unless governance documentation itself changed.
- Governance-only changes do not require changelog entries unless this
  repository intentionally starts tracking governance changes in the changelog.
- Documentation-only changes do not require a version bump.
- Read-only git commands may be run without asking:
  - `git status`
  - `git diff`
  - `git log`
  - `git show`
  - `git branch`
  - `git remote -v`
- Commands that modify history or working tree require confirmation:
  - `git add`
  - `git commit`
  - `git switch` / `git checkout`
  - `git reset`
  - `git merge`
  - `git rebase`
  - `git clean`
  - `git stash`
  - `git cherry-pick`
- Stage, commit, and push only on explicit user request or when a named
  workflow explicitly requires it.
- If staging is requested, prefer `git add -A`.
- Do not prepend `Set-Location` to git commands. Use the configured working
  directory. For non-git commands that must change directory, use
  `Push-Location` / `Pop-Location`.
- Prefer gh for PRs, CI checks, issues, and project operations when available.
- For `workflow.toMain`, commit, push, pull request creation, pull request
  merge, and branch cleanup are allowed only when explicitly requested by the
  shortcut and only after repository state, validation needs, and documentation
  impact have been checked.
- Before merging to `main`, run or report relevant validation, perform the
  documentation impact check, and report GitHub blockers such as required
  reviews, failing checks, conflicts, or branch protection.
- Owner/admin bypass may be used only when the user explicitly requested it for
  the current action. Required status checks must not be bypassed unless the
  user explicitly confirms that exception and the reason is reported.
- For `workflow.cleanBranches`, delete only branches verified as integrated.
  Do not delete active, unmerged, or ambiguous branches, and report skipped
  branches with the reason.

## Pull Request Review Policy

- Maintainer-owned PRs do not require external review by repository governance.
- Maintainer-owned PRs may be merged when required validation passed and GitHub
  branch protection allows the merge.
- External contributor PRs require review before merge. Do not merge unreviewed
  external contributions.
- Branch protection or ruleset bypass may only be used when the user explicitly
  requests owner/admin bypass for the current action.
- Do not use owner/admin bypass automatically.
- If GitHub branch protection blocks a merge because review is required, report
  the blocker and do not bypass branch protection.
- If a maintainer-owned PR is blocked only by a review requirement and required
  validation passed, report that owner/admin bypass may be possible when
  configured.
- If review is not intended for maintainer-owned PRs, recommend adjusting branch
  protection instead of bypassing it.
- Do not bypass external contributor PR review unless the user explicitly
  confirms a repository-owner exception after reviewing the PR.
- Required status checks must not be bypassed unless the user explicitly
  confirms an owner/admin exception and the reason is reported.

## Version Policy

- Require an appropriate project version bump for dependency updates,
  PlatformIO configuration changes, library metadata changes, firmware code
  changes, or example changes that affect build outputs unless the user
  explicitly says not to bump.
- Governance-only and documentation-only changes do not require a version bump.
  Report that the version bump was skipped by policy.
- Use patch version bumps for dependency updates, bug fixes, internal compatible
  changes, and build or configuration maintenance with no public API break.
- Use minor version bumps for new public features, new examples, new public
  APIs, and compatible behavior additions.
- Use major version bumps for breaking public API changes, incompatible
  storage/NVS layout changes, incompatible configuration schema changes, and
  behavior changes requiring user migration.
- `library.json` is the canonical source of truth for the esp32-bacnet-stack
  project/library version. Do not silently use README text, changelog text, or
  any example file as the source of truth.
- All other project/library version occurrences are mirrors and must be
  synchronized from `library.json` when the esp32-bacnet-stack project/library
  version changes.
- Before changing versions, release metadata, changelog/release notes, package
  metadata, library metadata, or files that may contain the project version,
  scan the repository for relevant version occurrences and report the candidate
  files found. The scan must include at least:
  - `library.json`
  - `README.md`
  - `docs/CHANGELOG.md` or the resolved changelog path when different
  - README version badges or version mentions, when present
  - any additional files found by ripgrep containing the current or target
    version string
- Suggested version scan commands:
  - `rg -n "version|VERSION|ESP_BACNET_VERSION|BACNET_STACK_VERSION" .`
  - `rg -n "<current-version>|<target-version>" .`
- The following known mirrors must carry the same esp32-bacnet-stack
  project/library version whenever the project version is changed:
  - `library.json`
  - README version badges or project/library version mentions, when present
  - changelog release headings or project/library version mentions, when
    present
  - example version references, when examples intentionally carry the
    library/app version
- If a required mirror path does not exist, report the missing path instead of
  silently ignoring it.
- The client and server examples are part of the library smoke/example baseline.
  If they gain explicit project/library version references, they must mirror the
  esp32-bacnet-stack project/library version.
- Other examples may have independent firmware/application versions. Do not
  automatically change them unless the issue explicitly asks for that example
  version to change. Preserve example-specific version policies and mention
  them in the report when relevant.
- TODO: If a Web UI or package-managed demo is added later, define whether its
  package metadata mirrors `library.json` or has an independent version before
  changing version files.
- Version changes that affect the published library/package must include a
  changelog update unless the governing issue explicitly says otherwise. Before
  editing changelog/docs, follow the docs-agent rules. If the changelog location
  is ambiguous, inspect the repository and report the resolved path.
- If version files disagree before work starts, report the mismatch before
  changing version-related files. If the intended target version is ambiguous,
  stop and ask for clarification.
- After any version-related change, report:
  - canonical version from `library.json`
  - all synchronized files and the version found in each
  - files intentionally not changed and why
  - commands used for version scanning
  - build/test validation performed
  - any remaining mismatch or risk

## Session-Close Workflow

- Default session-close behavior:
  - commit the current work branch (`feature/*`) with `git add -A` and a
    meaningful English commit message
  - push the work branch to `origin`
  - run `pio run -e usb` from the repository root when `.cpp` or `.h` files
    changed
  - if that build succeeds, or was skipped because no `.cpp` or `.h` files
    changed, update the active `release/*` branch to match the work branch
  - if no suitable release branch exists or no release branch is in scope,
    report that and skip the release update unless the user explicitly asks to
    create or update one
  - prefer fast-forward updates for the release branch
  - if fast-forward is not possible, ask explicitly before using
    `--force-with-lease`
  - push the updated release branch

## Code Rules

- Code comments must be in English.
- Identifiers and function names must be in English.
- Error and log messages must be in English.
- Emojis are forbidden in code, comments, logs, and generated outputs.
- Favor small, coherent changes over broad speculative refactors.
- Keep hardware-facing and configuration-sensitive changes conservative.

## Documentation Exception

- Markdown prose is exempt from flash-oriented logging brevity rules.
- Markdown prose may use readable long-form tags such as `[WARNING]`, `[NOTE]`,
  and `[INFO]`.
- Code blocks inside Markdown are not exempt. Code examples and text log
  examples must still follow the global logging policy from this file.
- Documentation must still be written in English unless the user explicitly
  requests a different language for a specific document.

## Logging Policy

All source code, headers, examples, tests, and code blocks inside Markdown must
follow this policy.

- Log messages must use short ASCII-only severity tags:
  - `[E]` error
  - `[W]` warning
  - `[I]` info
  - `[D]` debug
  - `[T]` trace
- Long tags such as `[ERROR]`, `[WARNING]`, `[INFO]`, `[DEBUG]`, and `[SUCCESS]`
  are forbidden in code/log output.
- Verbose is not a severity level. Verbose controls whether logs are emitted;
  severity tags describe impact.
- Severity reassignment is allowed when technically justified.
- Log message updates are not public API renames.
- Keep log text semantically clear and as short as reasonably possible to reduce
  flash usage.
- Prefer common abbreviations such as `cfg`, `init`, `ok`, `fail`, `adc`, `pwm`,
  and `io` where clarity remains intact.
- Do not repeat information already encoded by severity tags, module prefixes,
  or surrounding context.
- IO and hardware-level logs are usually `[D]` or `[T]`. Use `[W]` for notable
  recoverable conditions and `[E]` for critical failures.

## Rename And Logging Interaction

- API renames and logging normalization are separate concerns.
- Do not mix logging normalization into API rename phases.
- Before any API rename, perform a full reference search with rg.
- After renaming, rerun rg to ensure old names do not remain in relevant
  project files such as `src/`, `include/`, `lib/`, `test/`, `docs/`, and
  examples when present.

## Tool Policy

- Prefer available VS Code or agent-workspace tools first when they fit the task.
- Prefer locally installed CLI tools next.
- Do not use unnecessarily heavy tools when simple search or inspection is
  enough.
- Known local tools on the user's Windows/PowerShell environment may include:
  - git
  - gh
  - rg
  - fd
  - jq
  - dasel
  - jc
  - 7z
  - pwsh
  - winget
  - choco
  - coreutils
  - node / npm
  - python
  - uv
  - platformio / pio, if installed for this repository
- Preferred tools:
  - rg for text search, audits, and reference checks.
  - fd for file discovery, audits, and reference checks.
  - git for version-control inspection and explicit version-control actions.
  - platformio / pio for PlatformIO build, upload, monitor, and test flows.
  - gh for GitHub PRs, CI checks, issues, and project operations when
    available.
  - jq for JSON.
  - dasel for YAML, TOML, JSON, or XML inspection when it is the safest fit.
- When searching for governance, agent instructions, workflow shortcuts, branch
  rules, pull request rules, issue rules, validation rules, version rules, or
  repository policy, include hidden paths.
- For governance searches with rg, use `rg --hidden`.
- For governance discovery with fd, use `fd --hidden`.
- Plain `rg ... .` or plain `fd ... .` is not sufficient for governance
  discovery because hidden directories such as `.github/` may be skipped.
- When governance files are known by path, prefer reading those files directly
  over discovering them through repository-wide search.
- If rg or fd is missing, report that and give a simple install hint instead
  of silently using a weaker search path for audits or reference checks.
- When reporting search results, include the rg pattern used and state whether
  hidden paths were included when governance, policy, workflow, or agent files
  were in scope.
- Do not assume every listed tool is installed on every machine. If a required
  tool is missing, report the failed command clearly.
- Do not install or upgrade tools unless the user explicitly asks.
- On Windows, do not assume sed or awk are available. Prefer
  PowerShell-native commands unless availability was confirmed.
- Shell command quality:
  - Prefer simple robust commands over complex shell-escaped one-liners.
  - On Windows/PowerShell, prefer single quotes for regex/search patterns.
  - Avoid PowerShell backticks in search patterns unless required.
  - If quoting fails, retry once with safer quoting; do not repeat equivalent
    broken commands.
  - If quoting remains unclear, simplify, split the search, or read known files
    directly.
  - Do not treat failed commands as evidence about repository content.
  - Distinguish command failure, no matches, missing files, and missing tools in
    reports.
  - Example: `rg --hidden -n 'workflow\.begin|workflow\.audit' .github`.
- Use dasel for YAML, TOML, JSON, or XML inspection and edits when it is the
  safest fit.
- Use jq for JSON.
- Use gh for GitHub operations when authenticated and available.
- Use 7z for archive inspection or extraction when needed.
- Use uv only when Python tooling is actually part of the task. Do not infer
  Python project workflows from the presence of uv.
- Do not mix jq and dasel syntax.
- Do not use deprecated dasel flags.
- Prefer structured tools over brittle text parsing when that reduces risk.
- Do not assume repository, branch, PR, project, or governance state. Verify
  repository state with git and gh, and verify governance state by explicitly
  reading the relevant governance files.

## Validation Baseline

- Use configured and enabled GitHub Actions or checks when they exist.
- Do not invent required CI workflows.
- If no enabled CI is configured, report that and rely on required local
  validation.
- Always run at least one PlatformIO build after `.cpp` or `.h` changes.
- Default build check:
  - `pio run -e usb`
- For affected examples, run the relevant example build.
- If only Markdown or governance files changed, skip PlatformIO build unless the
  user asks for it.
- Governance-only changes require a governance consistency check, but do not
  require PlatformIO validation by themselves.
- A governance consistency check means reading the affected governance files and
  verifying that agent routing, shortcut rules, branch rules, tool policy,
  validation rules, version rules, and reporting rules do not contradict each
  other.
- If tests are affected, run `pio test` for at least one relevant environment.
- Run relevant tests when tests are present and affected.
- Docker or image builds are not required unless configured in this repository
  or explicitly in scope.
- If a required validation cannot run, report that plainly.

## Mandatory Reporting

After file-changing work, report:

- branch used
- governance files read, when governance, workflow, branch, PR, merge, release,
  validation, version, or cleanup rules were in scope
- files changed
- concise summary of what changed
- validation run
- skipped validation with reason
- remaining risks or blockers

Inspect relevant diffs before reporting file-changing work. Do not paste full
diffs into chat unless the user explicitly asks for the full diff. Prefer
`git diff --stat`, changed file lists, and focused summaries. Include focused
diff snippets only when needed to explain a risky, ambiguous, or important
change.

## Final Rule

- Never mark an issue as solved or fixed until the user explicitly confirms it
  works.

---

## .github/agents/control-plane.agent.md

# Control Plane Agent

Purpose:
Route each task to the correct repository agent. This file contains coordination
rules only.

Rules:

- Inspect `.github/AGENTS.md` first.
- Inspect available agent files under `.github/agents/`.
- Choose the repository agent that matches the current step:
  - `workflow.agent.md` for branch, issue, PR, release, checkpoint, and session
    workflows.
  - `refactor.agent.md` for code changes, refactoring, tests, and build
    validation.
  - `docs.agent.md` for documentation work.
- Multi-stage tasks may move between agents sequentially when the task naturally
  changes scope.
- If the selected task requires branch or publication decisions, route through
  `workflow.agent.md` before file-changing work starts.
- If agent selection is ambiguous, stop and report:
  - candidate agents
  - ambiguity reason
  - why selection is blocked
- Do not invent, simulate, or substitute a repository agent.
- Do not place project-specific code rules in this control-plane file.
- Follow the selected repository agent plus `.github/AGENTS.md` for actual task
  rules.

---

## .github/agents/docs.agent.md

# Documentation Agent

Purpose:
Keep documentation aligned with the implemented ESP32/PlatformIO system reality.

Use this agent for:

- `readme.md` updates
- `docs/` updates
- architecture or hardware wiring documentation
- release notes
- Wokwi usage documentation
- governance documentation

Scope:

- Document implemented behavior only.
- Prefer updating existing docs over creating parallel narratives.
- Do not make product-code changes from documentation work.
- If implementation truth is unclear, say so instead of guessing.
- If documentation would introduce a second conceptual model for the same
  subsystem, consolidate or stop and report the conflict.

Markdown Rules:

- Markdown prose may use readable long-form tags such as `[WARNING]`, `[NOTE]`,
  and `[INFO]`.
- Markdown prose is exempt from flash-oriented log brevity rules.
- Code blocks inside Markdown are not exempt. Code examples and text log
  examples must still follow the global logging policy from `.github/AGENTS.md`.
- Keep documentation concise and factual.
- Documentation may refer to GitHub Issues or Pull Requests when useful.
- Documentation must use the current configured GitHub Project from
  `.github/AGENTS.md` when project coordination is explicitly in scope.
- Project-board actions remain optional unless the user asks for tracked
  workflow or the task explicitly uses project coordination.
- Do not create a parallel task tracker in Markdown when GitHub Issues, PRs, or a
  configured GitHub Project are intentionally being used for the work.

Project Documentation Expectations:

- Update `readme.md` when project identity, setup, or local usage changes.
- Update `docs/` when hardware wiring, operational behavior, configuration, OTA,
  or architecture assumptions change.
- Keep references to PlatformIO commands aligned with `platformio.ini`.
- Mention environment-specific commands explicitly, for example `pio run -e usb`
  or `pio run -e ota`, when relevant.
- Before main integration, check documentation impact for changed files and
  behavior.
- If `docs/CHANGELOG.md` exists and a change is user-visible, release-relevant,
  dependency-related, build-related, or version-related, update the changelog or
  explicitly justify why no changelog update is needed.
- Do not invent documentation updates for purely internal or governance-only
  changes unless governance documentation itself changed.
- Governance-only changes do not require changelog entries unless this
  repository intentionally tracks governance changes in the changelog.
- Documentation-only changes do not require a version bump.

Reporting:

- Inspect relevant diffs before reporting documentation or governance changes.
- Do not paste full diffs into chat unless the user explicitly asks for the full
  diff.
- Prefer changed file lists, concise summaries, validation results, skipped
  validation reasons, and remaining risks or blockers.
- Include focused diff snippets only when needed to explain a risky, ambiguous,
  or important change.

Escalation:

- For branch sync, checkpoint, PR, or release needs, use `workflow.agent.md`.
- For structural code cleanup surfaced by docs conflicts, recommend
  `refactor.agent.md`.

---

## .github/agents/refactor.agent.md

# Refactor Agent

## Mandatory governance load preflight

Before executing any agent command, workflow shortcut, branch action, validation, file edit, or merge action, the agent MUST read the relevant governance files explicitly.

Required reads:

- `.github/AGENTS.md`
- `.github/agents/<selected-agent>.agent.md`

For workflow shortcuts, the selected agent is:

- `.github/agents/workflow.agent.md`

The agent MUST NOT rely on repository-wide search to discover these files, because `.github/` may be hidden from default search tools.

If using ripgrep to inspect governance, the agent MUST use `--hidden`, for example:

- rg --hidden -n "workflow\.begin|workflow\.audit|workflow\.toMain|workflow\.cleanBranches" .

## Purpose:
Perform safe C++/ESP32/PlatformIO code changes and structural improvements
without unintended behavior changes.

Use this agent for:

- C++ source and header changes under `src/`, `include/`, `lib/`, or `test/`
- internal refactors
- API renames
- logging normalization
- build and test validation
- PlatformIO configuration changes when explicitly in scope

## Scope:

- Preserve external behavior unless the user explicitly asks for behavior change.
- Keep changes small and coherent.
- Do not mix unrelated refactors into functional fixes.
- Do not change BACnet protocol semantics, network-facing behavior, storage/NVS
  layout, OTA behavior, security-sensitive behavior, or build pipelines without
  explicit confirmation.
- Keep hardware-facing changes conservative and easy to verify.

## Branch And Workflow:

- Use `workflow.agent.md` for branch creation, checkpointing, PRs, release
  updates, and branch cleanup.
- When a refactor task needs branch, issue, PR, or task-label wording, follow
  the user text normalization and branch-name derivation rules from
  `.github/AGENTS.md` and `workflow.agent.md`.
- Do not stage, commit, switch branches, merge, rebase, stash, clean, or push
  unless explicitly requested or directed by a workflow shortcut.
- If the active branch is `main` or `master` and file-changing work is
  requested, stop before editing and use the workflow rules to create or select
  a proper side branch.
- The only direct-edit exception on `main` or `master` is an explicitly
  requested docs-only TODO update under `docs/TODO.md` or `docs/todo_*.md`.
- Work on one side branch at a time.

## Version Handling:

- Follow the central Version Policy in `.github/AGENTS.md`.
- Refactor work must not change versions unless the central policy requires it
  or the user explicitly requests it.
- When refactor work touches versioning, release metadata, changelog/release
  notes, package metadata, library metadata, or files that may contain the
  project version, run and report a version scan before editing. Include at
  least these commands or stricter equivalents:
  - `rg -n "version|VERSION|ESP_BACNET_VERSION|BACNET_STACK_VERSION" .`
  - `rg -n "<current-version>|<target-version>" .`
- Treat `library.json` as the only canonical source of truth for the
  esp32-bacnet-stack project/library version. README text, changelog text, and
  example files are mirrors or independent app versions, not sources of truth.
- When the project/library version changes, synchronize the known mirror paths:
  `library.json`, README version badges or project/library version mentions,
  changelog release headings or project/library version mentions, and any
  example file containing the library/app version. Report any missing listed
  path.
- Keep client and server example version references aligned with the
  esp32-bacnet-stack project/library version if such references are added. Do
  not automatically change other examples' firmware/application versions unless
  the issue explicitly asks for it.
- TODO: If a Web UI or package-managed demo is added later, define package
  version mirror rules before changing its metadata.
- If version mirrors disagree before work starts, report the mismatch before
  changing version-related files. If the target version is unclear, stop and ask
  for clarification instead of guessing.
- After version-related changes, report the canonical `library.json` version,
  each synchronized file and the version found there, intentionally unchanged
  files, scan commands used, validation performed, and any remaining mismatch or
  risk.

## Rename Safety:

- Before any API rename, search all references with rg.
- After a rename, rerun rg to confirm old names do not remain in relevant
  locations.
- A rename is incomplete if old references remain in `src/`, `include/`, `lib/`,
  `test/`, `docs/`, or examples when present.
- Report the rg pattern used for reference checks.

## Logging Changes:

- Follow the global logging policy in `.github/AGENTS.md`.
- Keep API renames and logging normalization separate.
- Do not treat log text changes as public API renames.
- Hardware-level logs should default to `[D]` or `[T]` unless a higher severity
  is technically justified.

## Testing And Build Validation:

- Run at least one PlatformIO build after `.cpp` or `.h` changes.
- Default validation:
  - `pio run -e usb`
- For affected examples, run the relevant example build.
- If tests are affected, run `pio test` for at least one relevant environment.
- Run relevant tests when tests are present and affected.
- Prefer unit tests for core components when behavior is isolated enough to test.
- Use configured and enabled GitHub Actions or checks when they exist. Do not
  invent required CI workflows.
- If no enabled CI is configured, report that and rely on required local
  validation.
- Docker or image builds are not required unless configured in this repository
  or explicitly in scope.
- If OTA-specific behavior, upload configuration, or BACnet network behavior
  changes, validate the relevant PlatformIO environment as far as safely
  possible without assuming device availability.
- Mock implementations or mocked data used in tests must be clearly marked as
  `[MOCKED!]`.
- If validation cannot be run, report the reason plainly.

## Reporting:

- Inspect relevant diffs before reporting file-changing work.
- Do not paste full diffs into chat unless the user explicitly asks for the full
  diff.
- Prefer changed file lists, concise summaries, validation commands and results,
  skipped validation reasons, and remaining risks or blockers.
- Prefer `git diff --stat` or focused summaries for reporting.
- Include focused diff snippets only when needed to explain a risky, ambiguous,
  or important change.
- Follow the central Shell Command Quality rules when running search,
  validation, or audit commands.

## Strict Stops:

- Stop if behavior would change but the request was refactor-only.
- Stop if repository state, branch scope, or user edits make the safe edit path
  unclear.
- Stop before risky Level C work unless the user has explicitly confirmed it.

---

## .github/agents/workflow.agent.md

# Workflow Agent

Purpose:
Provide repository workflow rules for branches, issues, PRs, checkpoints,
release branches, and explicit session-close handling.

This agent must apply `.github/AGENTS.md`.

## Mandatory governance load

Before executing any agent command, workflow shortcut, branch action, validation, file edit, or merge action, the agent MUST read the relevant governance files explicitly.

Required reads:

- `.github/AGENTS.md`
- `.github/agents/<selected-agent>.agent.md`

For workflow shortcuts, the selected agent is:

- `.github/agents/workflow.agent.md`

The agent MUST NOT rely on repository-wide search to discover these files, because `.github/` may be hidden from default search tools.

If using ripgrep to inspect governance, the agent MUST use `--hidden`, for example:

- rg --hidden -n "workflow\.begin|workflow\.audit|workflow\.toMain|workflow\.cleanBranches" .

## Branch Model

- `main` is the published/released branch.
- `release/*` branches are runnable snapshot branches and must stay buildable and
  runnable.
- `release/*` branches are versioned by release, for example `release/v4.0.0`
  or `release/v4.1.0`.
- Do not assume `release/*` branches exist. If no suitable release branch
  exists, report that and skip release-branch updates unless the user explicitly
  asks to create or update one.
- Missing release branches do not block normal pull-request based `main`
  integration unless release sync is explicitly in scope.
- `feature/*` branches are work-in-progress branches and may be unfinished or
  temporarily broken.
- Work on one side branch at a time.
- Do not change `main` directly.
- Integrate into `main` through pull requests by default.
- Docs-only TODO updates under `docs/TODO.md` or `docs/todo_*.md` may be
  committed directly to `main` when the user explicitly requests that workflow.
- If file-changing work is requested while the active branch is `main` or
  `master`, stop before editing and create or select a proper side branch.
- The only direct-edit exception on `main` or `master` is an explicitly
  requested docs-only TODO update under `docs/TODO.md` or `docs/todo_*.md`.
- Direct pushes to `main` are forbidden unless the user explicitly requests an
  exception.
- Fast-forward integration to `main` is allowed only when the user explicitly
  requests fast-forward or `ff`.

## Git Command Rules

- Read-only git commands may be run without asking:
  - `git status`
  - `git diff`
  - `git log`
  - `git show`
  - `git branch`
  - `git remote -v`
- Commands that modify history or working tree require confirmation:
  - `git add`
  - `git commit`
  - `git switch` / `git checkout`
  - `git reset`
  - `git merge`
  - `git rebase`
  - `git clean`
  - `git stash`
  - `git cherry-pick`
- Stage, commit, and push only on explicit user request or when a named workflow
  explicitly requires it.
- If staging is requested, prefer `git add -A`.
- Do not prepend `Set-Location` to git commands. Use the configured working
  directory. For non-git commands that must change directory, use
  `Push-Location` / `Pop-Location`.

## Branch Safety

- Before multi-file refactors or risky changes, ensure the current baseline is
  understood and either clean, committed, or intentionally dirty by user request.
- Work incrementally: fix, verify, checkpoint or commit only when requested, then
  continue.
- Verify the active branch matches the task.
- If the active branch is `main` or `master`, warn and stop before
  file-changing work unless the direct-edit docs-only TODO exception applies.
- If branch naming does not match the task, warn and propose suitable branch
  names instead of silently switching.
- Never revert user edits unless explicitly asked.

## Branch Name And Title Derivation

- Derive branch names, issue titles, pull request titles, and task labels from
  the task intent, not by blindly copying informal free text.
- Correct obvious spelling mistakes in user-provided free text when the intended
  meaning is clear.
- Prefer short, clean English, lowercase, hyphen-separated branch names under
  the appropriate prefix, usually `feature/`.
- Do not silently correct exact file paths, commands, symbols, identifiers,
  branch names, issue numbers, pull request numbers, tags, versions, or quoted
  literals.
- If the user explicitly provides an exact branch name and says to use it
  exactly, preserve it as written.
- If the requested wording is ambiguous, or a correction would materially change
  the task scope, stop and ask before creating or switching branches.
- Check quoted paths against repository state before treating different casing
  as a typo. For example, `.github/agents.md` and `.github/AGENTS.md` may be a
  casing issue rather than interchangeable paths on every platform.
- Examples:
  - `use workflow.begin [gevernance-sharpenes]` may derive
    `feature/governance-sharpening`.
  - `use workflow.begin [update boilersaver example]` may derive
    `feature/update-boilersaver-example`.

## GitHub Workflow

- Prefer gh for PRs, CI checks, and issues when available.
- Keep PRs scoped to one coherent change.
- Do not claim merge readiness without running or reporting the relevant
  validation.
- Maintainer-owned PRs do not require external review by repository governance.
- Maintainer-owned PRs may be merged when required validation passed and GitHub
  branch protection allows the merge.
- External contributor PRs require review before merge. Do not merge unreviewed
  external contributions.
- Branch protection or ruleset bypass may only be used when the user explicitly
  requests owner/admin bypass for the current action.
- Do not use owner/admin bypass automatically.
- If GitHub branch protection blocks a merge because review is required, report
  the blocker and do not bypass branch protection.
- If a maintainer-owned PR is blocked only by a review requirement and required
  validation passed, report that owner/admin bypass may be possible when
  configured.
- If review is not intended for maintainer-owned PRs, recommend adjusting branch
  protection instead of bypassing it.
- Do not bypass external contributor PR review unless the user explicitly
  confirms a repository-owner exception after reviewing the PR.
- Required status checks must not be bypassed unless the user explicitly
  confirms an owner/admin exception and the reason is reported.
- GitHub Issue titles and bodies created by agents must be written in English.
- GitHub Pull Request titles and descriptions created by agents must be written
  in English.
- GitHub comments created by agents should be written in English unless the user
  explicitly requests otherwise.
- Internal chat with the user may remain informal German; do not use that as a
  reason to create German GitHub issue or PR text by default.
- Use GitHub Issues or PRs as task tracking when the user asks for tracked
  workflow, but do not invent mandatory project-board rules for this repository.
- GitHub Project usage is optional and controlled by the current configured
  project in `.github/AGENTS.md` `Tracking Policy`.
- Project-board actions remain optional unless the user asks for tracked
  workflow or the task explicitly uses project coordination.
- Issues and PRs remain allowed regardless of GitHub Project configuration.

## PlatformIO Workflow

- Use configured and enabled GitHub Actions or checks when they exist.
- Do not invent required CI workflows.
- If no enabled CI is configured, report that and rely on required local
  validation.
- Default build validation:
  - `pio run -e usb`
- For affected examples, run the relevant example build.
- OTA environment validation, when explicitly relevant:
  - `pio run -e ota`
- Upload commands require explicit user request because they interact with
  hardware or network devices.
- Serial monitor commands require explicit user request because they attach to
  local devices.
- If no `.cpp` or `.h` files changed, skip PlatformIO build unless requested.
- Run relevant tests when tests are present and affected.
- Docker or image builds are not required unless configured in this repository
  or explicitly in scope.

## Documentation Impact Workflow

- Before `workflow.toMain` prepares or merges changes into `main`, perform a
  documentation impact check.
- Check whether the changed files or behavior require updates to `README.md`,
  `docs/`, release notes, or governance documentation.
- If documentation is affected, route through `docs.agent.md` before merging.
- If `docs/CHANGELOG.md` exists and the change is user-visible,
  release-relevant, dependency-related, build-related, or version-related,
  update it or explicitly justify why no changelog update is needed.
- If documentation is not affected, report that documentation sync is not
  required and why.
- Do not invent documentation updates for purely internal changes.
- Governance-only changes do not require changelog entries unless this
  repository intentionally tracks governance changes in the changelog.
- Documentation-only changes do not require a version bump.

## Version Bump Workflow

- Dependency updates, PlatformIO configuration changes, library metadata
  changes, firmware code changes, and example changes that affect build outputs
  require an appropriate project version bump unless the user explicitly says
  not to bump.
- Governance-only and documentation-only changes do not require a version bump.
  Report that the version bump was skipped by policy.
- Use patch bumps for dependency updates, bug fixes, internal compatible
  changes, and build or configuration maintenance with no public API break.
- Use minor bumps for new public features, new examples, new public APIs, and
  compatible behavior additions.
- Use major bumps for breaking public API changes, incompatible storage/NVS
  layout changes, incompatible configuration schema changes, and behavior
  changes requiring user migration.
- Before changing versions, search for version declarations and report the
  candidate files found.
- If multiple version declarations exist, report them before changing versions.
- Treat the esp32-bacnet-stack package/library version in `library.json` as the
  main project version.
- TODO: If a Web UI or package-managed demo is added later, define whether its
  package metadata mirrors `library.json` or has an independent version before
  changing it.
- Bump an example firmware or app version only when that example itself is
  intentionally changed or released.
- If the version source of truth is unclear, stop and report candidate files
  instead of guessing.

## Release Branch Workflow

- `release/*` branches should represent runnable snapshots.
- Do not assume a release branch exists.
- If no suitable release branch exists, report that and skip release-branch
  update unless the user explicitly asks to create or update one.
- Missing release branches do not block PR-based `main` integration unless
  release sync was explicitly in scope.
- Prefer fast-forward updates when moving a release branch to a verified feature
  state.
- If fast-forward is not possible, ask explicitly before force-pushing. Prefer
  `--force-with-lease` if force-push is approved.
- Do not invent Python or `pyproject.toml` release steps for this repository
  unless the repository later adds such files and policy.
- Project version bumps are governed by `Version Bump Workflow`.

## Workflow Shortcuts

These names describe expected intent if the user invokes them:

- `workflow.begin`:
  - create the appropriate branch according to branch policy
  - derive a suitable clean English branch name from the task intent
  - avoid propagating obvious spelling mistakes from informal free text
  - preserve an exact branch name only when the user explicitly says to use that
    exact branch name
  - report the derived branch name before or immediately after creating it
  - mention when obvious typos were normalized, when relevant
  - do not perform code edits
  - do not perform build changes
  - do not run implementation work unless the user explicitly asks after the branch exists
- `workflow.checkpoint`: create a commit and push the current coherent state.
- `workflow.docs`: perform a narrow documentation-only synchronization.
- `workflow.audit`: read-only workflow or repository-state audit.
  - read-only only
  - no file changes
  - no branch changes
  - no commits
  - no merges
  - if followed by `workflow.toMain` or `workflow.cleanBranches`, finish the
    audit first and report blockers before any follow-up workflow runs
- `workflow.ship`: build and verify artifacts without implicit merge.
- `workflow.ready`: prepare work for review or integration, run or report
  relevant validation, do not merge to `main`, do not update `release/*`, and do
  not push unless explicitly requested or covered by a named workflow.
- `workflow.toMain`: get validated work onto `main` through the agreed pull
  request workflow unless the user explicitly requested fast-forward or `ff`;
  perform the documentation impact check before merging.
  - commit, push, PR creation, PR merge, and branch cleanup are allowed only as
    part of this explicitly requested workflow
  - run or report relevant validation before merge
  - perform the documentation impact check before merge
  - report GitHub blockers before merge, including required reviews, failing
    checks, conflicts, and branch protection
  - use owner/admin bypass only when the user explicitly requested it for the
    current action
  - do not bypass required status checks unless the user explicitly confirmed
    that exception and the reason is reported
- `workflow.cleanBranches`: delete only branches verified as integrated.
  - do not delete active, unmerged, or ambiguous branches
  - report skipped branches with the reason
- `workflow.end`: inspect repository state and report current branch, changed
  files, validation state, and blockers without claiming merge or fix success.
  Do not commit, push, merge, or update release branches unless explicitly
  requested.

Shortcut behavior must remain conservative:

- Inspect repository state first.
- Avoid destructive operations.
- Report blockers plainly.
- Do not create parallel branch lines for the same work.
- Chained shortcuts run sequentially. If audit blockers remain, stop before
  merge, cleanup, release updates, or destructive actions.
- Follow-up workflows after `workflow.audit` may run only when the user
  explicitly requested them and no blockers remain.
- Follow the central Shell Command Quality rules for search, validation, and
  audit commands.

## Mandatory Reporting

Report:

- current branch
- files changed when relevant
- concise summary of what changed
- git actions performed
- validation run
- validation skipped with reason
- release branch updates if any
- remaining blockers or risks

Inspect relevant diffs before reporting file-changing work. Do not paste full
diffs into chat unless the user explicitly asks for the full diff. Prefer
`git diff --stat`, changed file lists, and focused summaries. Include focused
diff snippets only when needed to explain a risky, ambiguous, or important
change.

---

