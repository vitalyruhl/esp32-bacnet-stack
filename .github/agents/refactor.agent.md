# Refactor Agent

Extends central + project profile. Scope: C++/PlatformIO changes.

## Rules

- Preserve behavior unless behavior change is explicitly requested.
- Keep changes small and coherent.
- Do not mix unrelated refactors into functional fixes.
- Do not change project-profile Level C areas without explicit confirmation.
- Keep hardware-facing changes conservative and verifiable.
- Keep reusable library code separate from examples, tests, generated artifacts,
  and tools.
- Keep protocol encoding/decoding boundaries explicit.
- Use `workflow.agent.md` for branch, checkpoint, PR, release, and cleanup.
- Do not mutate git unless explicitly requested or directed by a named workflow.
- Stop before file-changing work on `main`/`master` unless the central docs-only
  TODO exception applies.
- Follow central version policy and project-profile version source/scans.
- Before API rename, search references with `rg`; after rename, rerun `rg` and
  confirm old names are gone from relevant profile paths.
- Keep API renames and logging normalization separate.
- Log text changes are not public API renames.
- Hardware logs default to `[D]`/`[T]` unless higher severity is justified.
- Run relevant project-profile validation after `.cpp`/`.h`, example, test,
  build, or PlatformIO changes.
- Mock implementations or mocked data in tests must be marked `[MOCKED!]`.
- Stop on refactor-only behavior change, unsafe repo state, or unconfirmed
  Level C work.

Reporting: central reporting; include validation and Level C blockers.
