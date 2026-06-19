# Workflow Agent

Apply `.github/AGENTS.md` unchanged. This file adds only workflow-specific
rules for branches, issues, PRs, checkpoints, releases, cleanup, and
session-close handling.

## Branch Model

- `main` is the published/released branch.
- Integrate into `main` through PRs by default.
- Do not change `main` directly.
- If active branch is `main` or `master` and file-changing work is requested,
  stop before editing and create/select a side branch.
- Only exception: an explicitly requested docs-only TODO update under
  `docs/TODO.md` or `docs/todo_*.md`.
- Direct pushes to `main` are forbidden unless explicitly requested.
- Fast-forward integration to `main` is allowed only when explicitly requested
  as fast-forward or `ff`.
- `feature/*` branches are work-in-progress and may be unfinished or broken.
- `release/*` branches are runnable snapshots, versioned by release such as
  `release/v0.1.0`, and must stay buildable/runnable.
- Before the first release, `release/*` branches are optional.
- Do not assume a release branch exists. Missing release branches do not block
  normal PR-based `main` integration unless release sync is explicitly in scope.
- Create or update release branches only when explicitly requested.
- Work on one side branch at a time.
- Never revert user edits unless explicitly asked.

## Git Rules

- Read-only git commands may run without asking: `git status`, `git diff`,
  `git log`, `git show`, `git branch`, `git remote -v`.
- Mutating git commands require explicit confirmation: `git add`, `git commit`,
  `git switch`/`git checkout`, `git reset`, `git merge`, `git rebase`,
  `git clean`, `git stash`, `git cherry-pick`.
- Stage, commit, and push only on explicit user request or when a named workflow
  explicitly requires it.
- If staging is requested, prefer `git add -A`.
- Do not prepend `Set-Location` to git commands; use configured workdir.
- Verify active branch and repository state before mutating git actions.
- If branch naming does not match the task, warn and propose names instead of
  silently switching.

## Names And Titles

- Derive branch names, issue titles, PR titles, and task labels from task
  intent, not raw informal text.
- Correct obvious spelling mistakes when intent is clear.
- Prefer short, clean English, lowercase, hyphen-separated branch names under
  the right prefix, usually `feature/`.
- Preserve exact paths, commands, symbols, identifiers, branch names, issue/PR
  numbers, tags, versions, and quoted literals.
- Preserve exact branch names when the user says to use them exactly.
- Ask if wording is ambiguous or correction changes scope.
- Check quoted path casing against repository state.
- Example: `use workflow.begin [gevernance-sharpenes]` may derive
  `feature/governance-sharpening`.

## GitHub Workflow

- Prefer `gh` for PRs, CI checks, issues, and project operations when
  authenticated and available.
- Keep PRs scoped to one coherent change.
- Do not claim merge readiness without running or reporting relevant validation.
- Maintainer-owned PRs do not require external review by governance.
- External contributor PRs require review before merge.
- Owner/admin bypass is explicit-only for the current action.
- Required status checks MUST NOT be bypassed unless the user explicitly
  confirms the exception and the reason is reported.
- If branch protection blocks merge, report the blocker and do not bypass it.
- GitHub Issues, PRs, and comments created by agents follow the English language
  policy in `.github/AGENTS.md`.
- Use GitHub Issues, PRs, and the configured GitHub Project only when tracked
  workflow or project coordination is explicitly in scope.

## Validation And Docs

- Use configured/enabled GitHub Actions or checks when present; do not invent CI.
- Default PlatformIO build:
  - `pio run -e usb`
- Affected client example:
  - `pio run -d examples/client-demo -e usb`
- Affected server example:
  - `pio run -d examples/server-demo -e usb`
- Affected tests:
  - `pio test -e usb --without-uploading --without-testing`
- Explicitly relevant OTA:
  - `pio run -e ota`
- Upload and serial monitor commands require explicit user request.
- If only Markdown/governance files changed, skip PlatformIO unless requested.
- Before `workflow.toMain`, check documentation impact.
- If docs are affected, route through `docs.agent.md`.
- If `docs/CHANGELOG.md` exists and the change is user-visible,
  release-relevant, dependency-related, build-related, or version-related,
  update it or justify no update.
- Governance-only changes do not require changelog entries unless intentionally
  tracked there.
- Documentation-only and governance-only changes do not require a version bump.
- GitHub Actions-only Dependabot updates do not require a `library.json` bump
  unless they change produced library output, firmware build output, supported
  PlatformIO environments, or release artifact behavior.

## Release Branch Workflow

- `release/*` branches represent runnable snapshots.
- If no suitable release branch exists, report it and skip release update unless
  explicitly asked to create or update one.
- Prefer fast-forward updates when moving a release branch to verified feature
  state.
- If fast-forward is impossible, ask before force-pushing; prefer
  `--force-with-lease` if approved.
- Do not invent non-PlatformIO release steps.
- Version bumps follow `.github/AGENTS.md`.

## Shortcuts

- Shortcuts run sequentially. If `workflow.audit` blockers remain, stop before
  merge, cleanup, release updates, or destructive actions.
- Follow-up shortcuts after `workflow.audit` run only when explicitly requested
  and no blockers remain.
- `workflow.begin`: create/select the proper branch; derive a clean English name
  from task intent; preserve exact names only when requested; report the branch;
  do not edit code/build files or run implementation work unless explicitly
  requested after branch setup.
- `workflow.checkpoint`: verify branch and status; refuse direct `main`/`master`
  except the docs-only TODO exception; inspect `git diff --stat`; stage with
  `git add -A`; create one meaningful English commit; push only the current work
  branch; run or report relevant validation.
- `workflow.docs`: perform narrow documentation-only synchronization.
- `workflow.audit`: strictly read-only; no file changes, branch changes,
  commits, merges, cleanup, release updates, or destructive actions; report
  blockers before follow-up workflow.
- `workflow.ship`: build and verify artifacts without implicit merge.
- `workflow.ready`: prepare review/integration; run or report validation; do not
  merge to `main`, update `release/*`, or push unless explicitly requested or
  covered by a named workflow.
- `workflow.toMain`: use PR workflow by default unless fast-forward/`ff` is
  requested; commit, push, PR creation, PR merge, and cleanup are allowed only as
  part of this explicit workflow; run/report validation; perform docs impact
  check; report required reviews, failing checks, conflicts, and branch
  protection; use bypass only when explicitly requested; never bypass required
  status checks without explicit confirmed exception and reported reason.
- `workflow.cleanBranches`: delete only branches verified as integrated; skip
  active, unmerged, or ambiguous branches and report reasons.
- `workflow.end`: inspect state and report current branch, changed files,
  validation state, and blockers only; do not commit, push, merge, switch
  branches, update release branches, or claim merge/fix success.
- Session close is report-only by default. Do not commit, push, merge, switch
  branches, or update release branches during session close unless the user
  explicitly invokes `workflow.checkpoint`, `workflow.toMain`, or explicit
  release sync.

## Reporting

- Report current branch, changed files, git actions, validation run/skipped,
  release branch updates, and remaining blockers/risks.
- Inspect relevant diffs before reporting.
- Do not paste full diffs unless explicitly asked.
