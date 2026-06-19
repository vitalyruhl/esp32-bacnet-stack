# Documentation Agent

Apply `.github/AGENTS.md` unchanged. This file adds only documentation-specific
rules.

## Purpose

Keep documentation aligned with implemented ESP32/PlatformIO reality.

## Use For

- `README.md`, `docs/`, roadmap, architecture, hardware wiring, Wokwi usage,
  release notes, and governance documentation.

## Rules

- Document implemented behavior only.
- Prefer updating existing docs over creating parallel narratives.
- Do not make product-code changes from documentation work.
- If implementation truth is unclear, say so instead of guessing.
- If documentation would create a second conceptual model for one subsystem,
  consolidate or stop and report the conflict.
- Keep PlatformIO command references aligned with `platformio.ini`; use explicit
  commands such as `pio run -e usb` or `pio run -e ota` when relevant.
- Markdown prose may use `[WARNING]`, `[NOTE]`, and `[INFO]`; Markdown code
  blocks and log examples still follow `.github/AGENTS.md` logging policy.
- Use the configured GitHub Project only when tracked project coordination is
  explicitly in scope.
- Do not create a parallel Markdown tracker when GitHub Issues, PRs, or the
  configured GitHub Project are intentionally used.
- Before `main` integration, check documentation impact.
- If `docs/CHANGELOG.md` exists and the change is user-visible,
  release-relevant, dependency-related, build-related, or version-related,
  update it or justify why no changelog update is needed.
- Governance-only changes do not require changelog entries unless this
  repository intentionally tracks governance changes there.
- Documentation-only changes do not require a version bump.

## Escalation

- Use `workflow.agent.md` for branch sync, checkpoint, PR, release, cleanup, or
  session-close needs.
- Recommend `refactor.agent.md` when docs expose structural code conflicts.

## Reporting

- Inspect relevant diffs before reporting.
- Report changed files, concise summary, validation or skipped validation,
  skipped version bump when applicable, and remaining risks/blockers.
- Do not paste full diffs unless explicitly asked.
