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
- Before `workflow.toMain`, check docs impact. If docs are affected, route to
  `docs.agent.md`.
- If profile changelog exists and change is user-visible or
  release/dependency/build/version-related, update it or justify no update.
- Governance/docs-only changes need no version bump.

## Release

- `release/*` branches represent runnable snapshots.
- If no suitable release branch exists, report and skip release update unless
  explicitly asked to create/update one.
- Prefer fast-forward release updates.
- If fast-forward is impossible, ask before force-pushing; prefer
  `--force-with-lease` if approved.
- Do not invent non-PlatformIO release steps.
- Version bumps follow central + project profile.

## Shortcuts

- Shortcuts run sequentially. If `workflow.audit` blockers remain, stop before
  merge, cleanup, release updates, or destructive actions.
- Follow-up shortcuts after `workflow.audit` run only when explicitly requested
  and no blockers remain.
- `workflow.begin`: create/select proper branch; derive clean English name;
  preserve exact names only when requested; report branch; do not edit code/build
  files or run implementation work unless explicitly requested after setup.
- `workflow.checkpoint`: verify branch/status; refuse direct `main`/`master`
  except docs-only TODO exception; inspect `git diff --stat`; stage with
  `git add -A`; create one meaningful English commit; push only current work
  branch; run, skip, or reuse validation by central validation policy.
- `workflow.docs`: narrow documentation-only synchronization.
- `workflow.audit`: strictly read-only; no file changes, branch changes,
  commits, merges, cleanup, release updates, or destructive actions; report
  blockers before follow-up workflow.
- `workflow.ship`: build/verify artifacts without implicit merge.
- `workflow.ready`: prepare review/integration; run/skip/reuse validation; do
  not merge to `main`, update `release/*`, or push unless explicitly requested
  or covered by named workflow.
- `workflow.toMain`: PR workflow by default unless fast-forward/`ff` requested;
  commit/push/PR creation/merge/cleanup allowed only in this explicit workflow;
  run/skip/reuse validation; check docs impact; report reviews/checks/conflicts/
  branch protection; bypass only when explicitly requested; never bypass
  required checks without explicit confirmed exception and reported reason.
- `workflow.cleanBranches`: delete only branches verified integrated; skip
  active, unmerged, or ambiguous branches and report reasons.
- `workflow.end`: report branch, changed files, validation state, blockers only;
  do not commit, push, merge, switch branches, update release branches, or claim
  merge/fix success.
- Session close is report-only by default. Do not commit, push, merge, switch
  branches, or update release branches unless user invokes
  `workflow.checkpoint`, `workflow.toMain`, or explicit release sync.

Reporting: central reporting; include PR/merge/release blockers when relevant.
