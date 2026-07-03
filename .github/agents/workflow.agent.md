# Workflow Agent

Extends central + project profile. Scope: workflow.

Apply central Git, reporting, validation, and validation reuse rules; add
workflow-specific gates below.

## Branch Model

- `main` is published/released; integrate through PRs by default.
- Do not edit `main`/`master` directly except explicitly requested docs-only
  TODO updates under project-profile TODO paths.
- Direct pushes to `main` require explicit request.
- Fast-forward to `main` only when explicitly requested as fast-forward/`ff`.
- `feature/*` branches are work-in-progress and may be unfinished/broken.
- `release/*` branches are runnable snapshots and must stay buildable/runnable.
- Use the project-profile release example when needed.
- Before first release, `release/*` branches are optional.
- Do not assume a release branch exists; absence does not block PR-based `main`
  integration unless release sync is in scope.
- Create/update release branches only when explicitly requested.
- Work on one side branch at a time.
- Never revert user edits unless explicitly asked.

## Git And Names

- Verify branch/status before mutating git.
- If staging is requested, prefer `git add -A`.
- Do not prepend `Set-Location` to git commands; use configured workdir.
- If branch name mismatches task, warn and propose names instead of switching.
- Derive branch/issue/PR/task names from intent, not raw informal text.
- Correct obvious spelling mistakes when intent is clear.
- Prefer short clean English lowercase hyphenated branch names under the right
  prefix, usually `feature/`.
- Preserve exact paths, commands, symbols, identifiers, branch names, issue/PR
  numbers, tags, versions, and quoted literals.
- Preserve exact branch names when the user requests exact use.
- Ask if wording is ambiguous or correction changes scope.
- Check quoted path casing against repository state.
- Example: `use workflow.begin [gevernance-sharpenes]` may derive
  `feature/governance-sharpening`.

## GitHub And Docs

- Prefer `gh` for PRs, checks, issues, and project operations when authenticated.
- Keep PRs scoped to one coherent change.
- Do not claim merge readiness without validation passed/skipped/reused by
  central policy.
- Maintainer-owned PRs need no external review by governance.
- External contributor PRs require review before merge.
- Owner/admin bypass is explicit-only for the current action.
- Required checks MUST NOT be bypassed without explicit confirmed exception and
  reported reason.
- If branch protection blocks merge, report blocker and do not bypass.
- GitHub text created by agents follows central English policy.
- Use GitHub Issues/PRs/project only when tracked workflow is in scope.
- `workflow.agent.md` owns branches, PRs, merges, releases, checkpoints,
  cleanup, and general issue workflow.
- `plan.agent.md` owns planning-only issue breakdowns and tracked planning when
  explicitly requested.
- During `workflow.toMain`, invoke `docs.agent.md` before final merge/publish
  completion is considered valid. `workflow.toMain` may complete only after the
  docs gate passes or every docs finding is fixed or explicitly dispositioned by
  an allowed user instruction. `docs.agent.md` owns the documentation checklist
  and docs-gate report wording.
- If profile changelog exists and change is user-visible or
  release/dependency/build/version-related, update it or justify no update.
- Governance/docs-only changes need no version bump.


## GitHub Issue And Project Status

- Use the project-profile GitHub Project for tracked workflow coordination.
- `workflow.begin` and `.begin`/`.beginn` MUST detect issue numbers from the
  explicit user instruction, issue URL, branch name, prompt text, PR title/body,
  or existing branch/PR references when available.
- When begin starts issue-scoped work, move every detected active issue to
  `In Progress` in the project unless the user explicitly gives a different
  allowed status instruction.
- After moving an issue to `In Progress`, verify the project field value by
  reading it back. If verification fails, report a blocker and do not claim the
  issue is in progress.
- When supported by the current GitHub workflow, link or record branch/PR
  context for the detected issue. If the repository has an existing
  work-started comment or record rule, perform it and verify it. If no supported
  mechanism exists, report that no branch-link/comment mechanism is configured;
  do not invent one.
- If an issue-scoped begin cannot update Project status because of missing
  permissions, missing project item, ambiguous issue identity, API/tool failure,
  or field mismatch, report the blocker explicitly. Continue only if central
  governance allows continuing with a clear warning that project state is not
  synchronized.
- If begin includes multiple active issues, move all of them to `In Progress`
  unless the user explicitly excludes an issue or gives a different allowed
  status.
- If no issue can be detected for tracked workflow work, do not invent an issue
  and do not silently continue as tracked work. Ask whether to create a new
  issue, link an existing issue, or continue untracked if governance allows it.
  In non-interactive execution, stop and report a blocker.
- If an issue number is detected but the issue does not exist, is closed when
  active work is required, is missing from the Project, or is ambiguous, report
  the blocker and ask whether to create, link, reopen, update, or add the issue
  or Project item. Do not create issues or Project items without explicit
  permission.
- `workflow.toMain` MUST identify the issue or issues associated with the work
  started at `.begin`/`.beginn` by checking explicit user instructions, branch
  names, commits, PR links, closing keywords, issue references, and Project
  items.
- During `workflow.toMain`, verify commits, branch names, PR metadata, and issue
  references before changing issue or Project status.
- The default target Project status after a valid `workflow.toMain` is `Done`
  for all detected issues from the begin-to-toMain work scope unless the user
  explicitly says otherwise.
- Explicit user status instructions win over the default target, for example
  keeping an issue `In Review`, moving an issue to `In Progress`, not closing a
  specific issue, or closing only selected issues.
- The central final rule still applies: do not mark an issue solved/fixed, close
  it, or move it to a final status unless the user confirmation requirement is
  satisfied. If this blocks the default `Done` target, report the stricter
  blocker and the issue-specific skipped update.
- No issue/Project status update may be skipped silently. Every detected issue
  MUST be reported with old status, intended status, final status, whether the
  issue was closed or only moved in Project, and the reason for any skipped or
  failed update.
- Governance-only workflow changes do not require GitHub Issue tracking when the user explicitly allows untracked governance work. In that case, report `Issue tracking skipped: governance-only work explicitly allowed untracked by user.` and do not create or update GitHub Issues or Project items.

## Release

- `release/*` branches represent runnable snapshots.
- If no suitable release branch exists, report and skip release update unless
  explicitly asked to create/update one.
- Prefer fast-forward release updates.
- If fast-forward is impossible, ask before force-pushing; prefer
  `--force-with-lease` if approved.
- Do not invent non-PlatformIO release steps.
- Version bumps follow central + project profile.

## Version Impact Gate

- `workflow.begin` MUST classify version impact for issue-scoped product work and
  report one of these outcomes before implementation starts:
  - `Version impact: required, expected bump: patch|minor|major, reason`
  - `Version impact: not required, reason`
  - `Version impact: uncertain, blocker or explicit follow-up required`
- Version-impact classification is required when scope clearly touches product
  code, public API, PlatformIO config, build matrix/metadata, examples that
  affect build output, dependencies, or release artifacts.
- PlatformIO config and build matrix changes are version-impacting by default
  unless the task is explicitly docs/governance-only or covered by the existing
  GitHub Actions-only dependency-update exception.
- `workflow.begin` is setup-oriented and MUST NOT edit version files by default
  unless the user explicitly requests implementation in the same prompt.
- `workflow.checkpoint` MUST verify that required version bumps are present
  before commit when product/build/API/version-impacting changes are included.
- `workflow.toMain` MUST block merge readiness when a required version bump is
  missing.
- Intentional version-bump deferrals are blockers unless the user explicitly
  instructs no version bump for the current action.
- Governance/docs-only changes keep version bump skipped with explicit reason.

## Shortcuts

- Shortcuts run sequentially. If `workflow.audit` blockers remain, stop before
  merge, cleanup, release updates, or destructive actions.
- Follow-up shortcuts after `workflow.audit` run only when explicitly requested
  and no blockers remain.
- `workflow.begin` / `.begin` / `.beginn`: create/select proper branch; derive
  clean English name; preserve exact names only when requested; detect linked
  issue scope; for issue-scoped work, move every active issue to `In Progress`,
  verify the Project status read-back, and report old/new status; report a
  blocker if Project status cannot be updated; report branch; apply Version
  Impact Gate classification for issue-scoped product work; do not edit
  code/build/version files or run implementation work unless explicitly
  requested after setup.
- `workflow.checkpoint`: verify branch/status; refuse direct `main`/`master`
  except docs-only TODO exception; inspect `git diff --stat`; stage with
  `git add -A`; create one meaningful English commit; push only current work
  branch; run, skip, or reuse validation by central validation policy in this
  order: branch/status/diff inspection, autofix-capable validation first,
  `git status --short`, rerun until clean/no modifications, expensive
  project-profile PlatformIO validation, final `git status --short`, then
  commit readiness reporting. For any touched `.c`, `.cc`, `.cpp`, `.cxx`,
  `.h`, `.hh`, or `.hpp` files, `cppcheck` must be run through `pre-commit`
  before expensive PlatformIO validation and reported as passed, reused, or
  explicitly blocked with a reason; enforce Version Impact Gate requirements
  and block checkpoint when a required version bump is missing. For
  `workflow.checkpoint`, validation must be performed before the final staged
  commit state is accepted. If autofix-capable validation modifies files, the
  workflow MUST stop before commit, report the modified files, inspect the diff,
  rerun the same autofix-capable validation until it exits cleanly, and only
  then accept PlatformIO pass/reuse and stage/commit the final no-op state.

Before creating the commit, run and report `git status --short` after the final
validation sequence. A checkpoint commit MUST NOT be created from a state where
the last validation run failed, was autofixed, or left unreported file
modifications.
- `workflow.docs`: narrow documentation-only synchronization.
- `workflow.audit`: strictly read-only; no file changes, branch changes,
  commits, merges, cleanup, release updates, or destructive actions; report
  blockers before follow-up workflow.
- `workflow.ship`: build/verify artifacts without implicit merge.
- `workflow.ready`: prepare review/integration; inspect branch/status/diff, run
  autofix-capable validation first, run `git status --short`, rerun until clean,
  then run or validly reuse expensive project-profile PlatformIO validation for
  the final file state, run final `git status --short`, and only then report
  readiness. Do not merge to `main`, update `release/*`, or push unless
  explicitly requested or covered by named workflow.
- `workflow.toMain`: PR workflow by default unless fast-forward/`ff` requested;
  commit/push/PR creation/merge/cleanup allowed only in this explicit workflow;
  run/skip/reuse validation in this order: branch/status/diff inspection,
  autofix-capable validation first, `git status --short`, rerun until clean/no
  modifications, expensive project-profile PlatformIO validation, final
  `git status --short`, then PR/merge readiness reporting; run `docs.agent.md`
  and require the docs gate to pass or have findings resolved; the
  project-profile mandatory test command is required for this workflow and must
  run after the latest autofix modifications unless reuse is valid under
  central validation reuse rules; for any touched `.c`, `.cc`, `.cpp`, `.cxx`,
  `.h`, `.hh`, or `.hpp` files, `cppcheck` through `pre-commit` must run before
  expensive PlatformIO validation unless explicitly blocked and reported;
  enforce Version Impact Gate and block merge readiness when required version
  bump changes are missing; identify associated issues; update GitHub Project
  status according to the default/explicit rules above; report reviews, checks,
  conflicts, branch protection, issue status updates, and docs results; bypass only when
  explicitly requested or already allowed by governance for the current action;
  never bypass required checks without explicit confirmed exception and
  reported reason. Owner/admin bypass does not weaken final state requirements:
  local `main` MUST equal `origin/main`, the working tree MUST be clean, and no
  leftover local, remote, or stale-tracking non-main work branches may remain
  unless explicitly preserved. Fast-forward/`ff` MUST preserve linear history
  where expected and must satisfy the same final synchronization and cleanup
  requirements.
  During `workflow.toMain`, only list validation commands as passed when they
  were actually executed after the latest relevant file changes, or when reuse
  is valid under central validation reuse rules.

  If a validation command is only covered indirectly by a wrapper, report it as
  wrapper-covered instead of listing it as a separately executed command.

  Before PR creation and again before merge readiness, verify that the final
  file state passed the autofix-first validation order and that the working tree
  is clean after validation. If validation modifies files, commit those changes
  through the same PR workflow before claiming merge readiness.
- `workflow.cleanBranches`: delete only branches verified integrated; include
  both local and remote cleanup; delete already-integrated remote
  feature/work branches when safe and allowed; fetch/prune tracking refs after
  remote cleanup; skip active, unmerged, explicitly protected, release, main, or
  ambiguous branches and report reasons. If a stale local, remote, or
  tracking branch cannot be deleted or pruned, stop or report it as an explicit
  blocker; do not silently leave stale branches behind.
- `workflow.end`: report branch, changed files, validation state, blockers only;
  do not commit, push, merge, switch branches, update release branches, or claim
  merge/fix success.
- Session close is report-only by default. Do not commit, push, merge, switch
  branches, or update release branches unless user invokes
  `workflow.checkpoint`, `workflow.toMain`, or explicit release sync.

## Final Synchronization Checks

After `workflow.toMain` followed by required cleanup, the repository MUST be in
a synchronized valid state:

- local `main` exists
- remote `origin/main` exists
- local `main` HEAD equals `origin/main` HEAD
- current branch and ahead/behind state are known and verified
- working tree is clean
- local branch list contains no remaining feature/work branches unless
  explicitly protected by user instruction
- remote branch list contains no remaining feature/work branches unless
  explicitly protected by user instruction
- no stale local tracking branches remain after fetch/prune
- no merged feature branch remains locally or remotely unless explicitly kept by
  user instruction
- only `main` remains as the normal active project branch

The workflow MUST explicitly verify current branch, `git status`, local `main`
HEAD, `origin/main` HEAD, ahead/behind state, local branches after cleanup,
remote branches after cleanup, pruned tracking refs, and confirmation that local
and remote are synchronized.

Successful routine reports MUST summarize those checks compactly:

- final result: success or failure
- final branch, normally `main`
- working tree: clean or dirty
- local/remote sync: synced or not synced
- branch cleanup: completed, skipped with reason, or blocked
- docs gate: passed, no changes needed, findings resolved, or explicitly
  dispositioned
- validation: passed, skipped according to policy, or reused according to policy
- issues/Project: issue number, old status, final status, and closed or
  Project-only action
- PR/merge result when a PR was used
- explicit preserved branches, if any
- blockers, if any

Do not report commit hashes, local `main` HEAD hash, `origin/main` HEAD hash,
full branch lists, full command output, detailed API responses, repeated
validation logs, or implementation summaries already covered by the commit/PR
on successful routine runs unless the user asks for details.

Detailed synchronization data MUST be reported when the user requests details,
verification fails, local `main` and `origin/main` differ, branch cleanup cannot
complete, a branch is ambiguous or preserved, a PR/merge/release blocker occurs,
issue/Project status update fails, or the workflow needs an audit trail to
explain a blocker. If any check fails, report the failing condition as a
blocker.

## Acceptance Criteria

`workflow.begin` / `.begin` / `.beginn` acceptance criteria:

- branch created or selected according to existing branch/name rules
- issue detection attempted from explicit instruction, branch, prompt, and
  available GitHub references
- for issue-scoped work, every active detected issue moved to `In Progress`
  unless explicitly overridden by the user
- Project status update verified by read-back
- blocker reported if status could not be updated or verified
- branch and issue/Project result reported
- successful begin output remains concise; detailed GitHub/API output is
  omitted unless issue detection or status update fails, the user asks for
  details, or the details are needed to explain a blocker

`workflow.toMain` acceptance criteria:

- required validation passed, was reused only according to central validation
  rules, or was explicitly blocked with a reported reason; `pio test -e usb
  --without-uploading --without-testing` is mandatory for this workflow and must
  not be skipped silently; if the work touches `.c`, `.cc`, `.cpp`, `.cxx`, `.h`,
  `.hh`, or `.hpp` files, `cppcheck` through `pre-commit` is also mandatory and
  must not be skipped silently
- `docs.agent.md` passed, or every docs finding was resolved before completion
- associated issues detected and verified against branch, commits, PR metadata,
  and explicit user instructions
- GitHub Project statuses updated according to default/explicit rules, without
  weakening the central user-confirmation rule for solved/fixed issues
- issue close-vs-Project-only action reported for every detected issue
- Final Synchronization Checks completed as the canonical synchronization,
  branch cleanup, stale-tracking, and compact-reporting rule set
- successful routine report is concise and omits hashes, full branch lists, full
  command output, repeated validation logs, and detailed API responses unless
  the user asks for details
- detailed synchronization data is available on request and automatically
  included for blockers, failures, skipped required actions, divergent
  local/remote state, ambiguous branches, preserved branches, or failed
  issue/Project updates
- no untracked governance changes are left behind

`workflow.cleanBranches` acceptance criteria:

- local cleanup completed for all integrated, unprotected work branches
- remote cleanup completed for all integrated, unprotected remote work branches
- fetch/prune verification completed after remote cleanup
- successful routine cleanup may be reported as completed without full branch
  lists
- full local and remote branch lists reported only when branches remain, cleanup
  fails, branches are preserved, branch identity is ambiguous, or the user asks
  for details
- explicit exception list reported for every preserved branch, including reason

Reporting: use central reporting. Keep successful routine output compact;
include low-level details only on request or when needed to explain a blocker.
