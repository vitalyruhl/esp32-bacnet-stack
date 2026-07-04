# Contributing Guidelines

These guidelines define how to work in this repository. The project targets
ESP32, PlatformIO, Arduino, and C++17.

## 1. Where things live

- Documentation: `docs/`
- Tests: `test/`
- Examples: `examples/`
- Tools and helper scripts: `tools/`
- GitHub files: `.github/`
- Keep the repository root limited to essential project files.

## 2. Branching model

- `main`: published or released branch. Do not develop directly here.
- `release/*`: runnable snapshot branches when they exist.
- `feature/*`: work-in-progress branches.
- Suggested naming:
  - `feature/<short-topic>`
  - `fix/<short-topic>`
  - `chore/<short-topic>`
  - `docs/<short-topic>`

## 3. Code style

- Code comments, identifiers, and log/error messages must be English.
- Do not use emojis in code, comments, logs, or generated outputs.
- Use short ASCII severity tags in logs: `[I]`, `[W]`, `[E]`, `[D]`, `[T]`.
- Prefer C++17 features where they keep the code clear.
- Keep hardware-facing behavior conservative and easy to verify.

## 4. Public API direction

- The core public roles are `BacnetClient` and `BacnetServer`.
- Avoid adding duplicate public role names for the same concept.
- Before any API rename, search all references and update source, examples,
  docs, and tests together.
- The project is not yet public, so early API cleanup may be direct, but it
  still needs full reference checks.

## 5. Documentation requirements

- Document implemented behavior only.
- Keep PlatformIO commands aligned with `platformio.ini`.
- Mention upload and monitor commands separately from default validation because
  they interact with local hardware.
- Keep BACnet/IP and future BACnet MS/TP status clear.

## 6. Testing and build validation

- Default root build: `pio run -e usb`
- Compile-only tests: `pio test -e usb --without-uploading --without-testing`
- Client demo build: `pio run -d examples/client-demo -e usb`
- Server demo build: `pio run -d examples/server-demo -e usb`
- Full local governance gate: `pwsh -NoProfile -ExecutionPolicy Bypass -File tools/run-precommit-full.ps1`
- Direct static analysis gate: `pwsh -NoProfile -ExecutionPolicy Bypass -File tools/run-cppcheck.ps1`

### Cppcheck policy

1. `error`, `warning`, `performance`, `portability`, and `style` findings are mandatory-fix by default.
2. The mandatory gate runs on the severity set above; `information` findings are triaged separately and tool-status-only messages do not require code changes.
3. `unmatchedSuppression` findings must be fixed by removing or correcting stale suppressions.
4. Suppressions are allowed only for confirmed false positives, must be as narrow as possible, and must include a clear reason near the suppression.
5. Prefer fixing code over adding suppressions.
6. Do not weaken cppcheck globally just to pass the gate.
7. If a safe fix is not possible in the current slice, open a follow-up issue and document the risk and justification.

## 7. Dependency maintenance

- GitHub Actions updates are managed by Dependabot.
- PlatformIO dependency updates are manual unless GitHub Dependabot adds an
  official PlatformIO ecosystem later.

## 8. Git workflow etiquette

- Prefer small, reviewable commits.
- Do not stage, commit, push, merge, rebase, reset, clean, stash, or switch
  branches unless explicitly requested by the current task or workflow.
- Before rename or delete operations, search references and update them.

## 9. CI and merge policy

- All configured CI checks must pass before merging to `main`.
- Do not add deployment or publishing steps to CI unless explicitly requested.
- Keep `README.md` and `docs/` aligned when usage or behavior changes.
- Keep [repository settings](repository-settings.md) updated when GitHub
  security, project, or branch-protection settings change.
