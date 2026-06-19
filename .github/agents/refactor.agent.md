# Refactor Agent

Apply `.github/AGENTS.md` unchanged. This file adds only C++/ESP32/PlatformIO
refactor rules.

## Purpose

Perform safe code changes and structural improvements without unintended
behavior changes.

## Use For

- C++ source/header changes under `src/`, `include/`, `lib/`, or `test/`.
- Internal refactors, API renames, logging normalization, tests, examples, build
  validation, and PlatformIO configuration when explicitly in scope.

## Scope Rules

- Preserve external behavior unless behavior change is explicitly requested.
- Keep changes small and coherent.
- Do not mix unrelated refactors into functional fixes.
- Do not change BACnet protocol semantics, network-facing behavior, storage/NVS
  layout, OTA behavior, security-sensitive behavior, build pipelines, or large
  refactors without explicit confirmation.
- Keep hardware-facing changes conservative and easy to verify.
- Keep reusable BACnet stack code separate from examples, tests, generated
  artifacts, and repository tooling.
- Keep protocol encoding/decoding boundaries explicit.

## Workflow

- Use `workflow.agent.md` for branch creation, checkpointing, PRs, release
  updates, and cleanup.
- Do not stage, commit, push, switch branches, merge, rebase, reset, stash,
  clean, delete files, or update release branches unless explicitly requested or
  directed by a named workflow.
- If active branch is `main` or `master`, stop before file-changing work unless
  the docs-only TODO exception in `.github/AGENTS.md` applies.

## Version Handling

- Follow `.github/AGENTS.md` Version Policy.
- Do not change versions unless central policy requires it or the user
  explicitly requests it.
- Before version-related edits, run/report:
  - `rg -n "version|VERSION|ESP_BACNET_VERSION|BACNET_STACK_VERSION" .`
  - `rg -n "<current-version>|<target-version>" .`
- Treat `library.json` as the only canonical esp32-bacnet-stack
  project/library version.
- Synchronize required mirrors from `library.json`; report missing paths,
  intentional non-mirrors, mismatches, scan commands, validation, and risks.
- Keep client/server example version references aligned if such references are
  added. Do not change other example app versions unless explicitly requested.

## Rename And Logging

- Before API rename, search all references with `rg`.
- After rename, rerun `rg` and confirm old names do not remain in `src/`,
  `include/`, `lib/`, `test/`, `docs/`, or examples when present.
- Report the `rg` pattern used.
- Follow `.github/AGENTS.md` logging policy.
- Keep API renames and logging normalization separate.
- Log text changes are not public API renames.
- Hardware logs default to `[D]` or `[T]` unless higher severity is technically
  justified.

## Validation

- After `.cpp` or `.h` changes, run:
  - `pio run -e usb`
- For affected client example changes:
  - `pio run -d examples/client-demo -e usb`
- For affected server example changes:
  - `pio run -d examples/server-demo -e usb`
- For affected tests:
  - `pio test -e usb --without-uploading --without-testing`
- For explicitly relevant OTA behavior/configuration:
  - `pio run -e ota`
- Use configured GitHub Actions/checks when present; do not invent CI.
- Mock implementations or mocked data in tests must be marked `[MOCKED!]`.
- If validation cannot run, report the reason plainly.

## Strict Stops

- Stop if behavior would change during refactor-only work.
- Stop if repository state, branch scope, or user edits make the safe path
  unclear.
- Stop before Level C/risky work without explicit confirmation.

## Reporting

- Inspect relevant diffs before reporting.
- Report changed files, summary, validation, skipped validation reasons, version
  handling when relevant, and risks/blockers.
- Do not paste full diffs unless explicitly asked.
