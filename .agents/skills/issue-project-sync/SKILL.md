---
name: issue-project-sync
description: Canonical explicit GitHub Issue and Project synchronization procedure with read-back verification.
user-invocable: false
---

# Issue Project Sync

Use only for explicitly tracked workflow or planning. Use the project profile's
Project and `gh` when authenticated. Do not invent an issue or Project update.

For `workflow.begin`, detect explicit issue references from the request, URL,
branch, prompt, PR title/body, and existing branch/PR references. For each
detected active issue, set the allowed target (normally `In Progress`) and read
it back. If identity, permissions, Project membership, field mapping, API, or
read-back fails, report a blocker and do not claim synchronization. Continue
only when the coordinator explicitly allows an unsynchronized warning. Do not
create, reopen, link, comment on, or add Issue/Project items without explicit
permission. If no issue is detectable for tracked work, stop and ask whether to
create, link, or continue untracked.

For `workflow.toMain`, identify associated issues from begin scope, explicit
instructions, branches, commits, PR links/closing keywords, and Project items;
verify those references before mutation. The normal target is `Done` unless the
user authorizes a different state. Never close an issue, mark it solved/fixed,
or move it to final status without the user's confirmation that it works. Report
every issue as old state, intended state, final state, close-vs-Project-only
action, and any skip/failure reason. Governance-only work explicitly authorized
as untracked states: `Issue tracking skipped: governance-only work explicitly
allowed untracked by user.`
