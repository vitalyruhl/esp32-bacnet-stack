# Project Profile

Mandatory project-specific context for all repository agents. This file is not
a work agent and does not replace `.github/AGENTS.md` or the selected agent
overlay.

## Identity

- Repository name: `esp32-bacnet-stack`.
- Project description: C++ ESP32 BACnet/IP stack library.
- Framework/build: PlatformIO with the Arduino framework.
- Canonical library version source: `library.json`.
- Current GitHub Project: `ESP32 BACnet Stack` (#6).

## Primary Files And Areas

- Primary configuration: `platformio.ini`.
- Canonical library metadata and version source: `library.json`.
- Main library code: `src/`.
- Public headers: `include/` when present.
- Internal libraries: `lib/` when present.
- Examples: `examples/`.
- Client example: `examples/client-demo`.
- Server example: `examples/server-demo`.
- Tests: `test/` when present.
- Documentation: `README.md` and `docs/`.
- Changelog path: `docs/CHANGELOG.md` when present.
- Docs-only TODO paths: `docs/TODO.md` and `docs/todo_*.md`.
- Tooling: `tools/`.
- Serena metadata: `.serena/`; memories are summaries only and never override
  repository files, user instructions, `.github/AGENTS.md`, or this profile.
- Wokwi files may live under example-specific `Wokwi/` folders.

## Architecture Facts

- The library exposes BACnet/IP client and server roles.
- Main roles/classes: `BacnetClient`, `BacnetServer`.
- Keep reusable BACnet stack code separate from examples, tests, generated
  artifacts, and repository tooling.
- Keep protocol encoding and decoding boundaries explicit.
- Do not add core dependencies on optional demo integrations.
- Do not import external BACnet implementation code without explicit license,
  provenance, and architecture review.

## Risk Profile

Treat these areas as Level C/risky:

- BACnet protocol semantics
- network-facing behavior
- storage/NVS
- OTA
- security
- PlatformIO/build pipelines
- large refactors

## Version Policy Details

- Use `library.json` as the canonical esp32-bacnet-stack project/library
  version.
- README text, changelog text, and examples are mirrors or independent app
  versions, not sources of truth.
- Version scan commands:
  - `rg -n "version|VERSION|ESP_BACNET_VERSION|BACNET_STACK_VERSION" .`
  - `rg -n "<current-version>|<target-version>" .`
- Candidate version files include:
  - `library.json`
  - `README.md`
  - `docs/CHANGELOG.md` when present
  - README version badges or project/library version mentions when present
  - files found by ripgrep containing the current or target version
- When the project/library version changes, synchronize `library.json`, README
  version badges/mentions, changelog headings/mentions, and examples that
  intentionally carry the library/app version.
- Keep client and server example version references aligned with the
  esp32-bacnet-stack project/library version if such references are added.
- Do not automatically change other example firmware/application versions
  unless the issue explicitly asks for that example version to change.
- GitHub Actions-only Dependabot updates do not require a `library.json` version
  bump unless they change produced library output, firmware build output,
  supported PlatformIO environments, or release artifact behavior.

## Validation Commands

- Default PlatformIO environment: `usb`.
- Optional OTA environment: `ota`.
- Root build:
  - `pio run -e usb`
- Client example:
  - `pio run -d examples/client-demo -e usb`
- Server example:
  - `pio run -d examples/server-demo -e usb`
- Tests:
  - `pio test -e usb --without-uploading --without-testing`
- OTA when explicitly relevant:
  - `pio run -e ota`
- Upload and serial monitor commands require explicit user request.

## Tracking And Release

- Use GitHub Project `ESP32 BACnet Stack` (#6) only when tracked workflow or
  project coordination is explicitly in scope.
- Release branch example: `release/v0.1.0`.
- Release branches are optional before the first release.
