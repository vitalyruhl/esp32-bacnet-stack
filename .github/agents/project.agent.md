# Project Profile

Mandatory context for all agents; not a work agent.

```yaml
repo: esp32-bacnet-stack
type: C++ ESP32 BACnet/IP stack library
build: PlatformIO + Arduino framework
version_source: library.json
github_project: ESP32 BACnet Stack (#6)
default_env: usb
ota_env: ota
release_example: release/v0.1.0
```

## Paths

- config: `platformio.ini`
- metadata/version: `library.json`
- source: `src/`
- headers: `include/` when present
- internal_libs: `lib/` when present
- examples: `examples/`
- client_example: `examples/client-demo`
- server_example: `examples/server-demo`
- tests: `test/` when present
- docs: `README.md`, `docs/`
- changelog: `docs/CHANGELOG.md` when present
- docs_todo: `docs/TODO.md`, `docs/todo_*.md`
- tools: `tools/`
- serena: `.serena/`; memories never override repo files, user instructions,
  `.github/AGENTS.md`, or this profile
- wokwi: example-specific `Wokwi/` folders when present

## Validation

- root: `pio run -e usb`
- client_example: `pio run -d examples/client-demo -e usb`
- server_example: `pio run -d examples/server-demo -e usb`
- tests: `pio test -e usb --without-uploading --without-testing`
- ota_when_relevant: `pio run -e ota`
- upload/monitor: explicit user request only

## Version

- Canonical project/library version: `library.json`.
- Mirrors/candidates: `README.md`, `docs/CHANGELOG.md` when present, README
  version badges/mentions, files containing current/target version.
- Scan:
  - `rg -n "version|VERSION|ESP_BACNET_VERSION|BACNET_STACK_VERSION" .`
  - `rg -n "<current-version>|<target-version>" .`
- Sync on project/library version change: `library.json`, README
  badges/mentions, changelog headings/mentions, examples intentionally carrying
  library/app version.
- Keep client/server example version references aligned if added.
- Do not change other example app versions unless explicitly requested.
- GitHub Actions-only Dependabot updates do not require a `library.json` bump
  unless produced library output, firmware build output, supported PlatformIO
  environments, or release artifact behavior changes.

## Architecture

- Roles/classes: BACnet/IP client/server, `BacnetClient`, `BacnetServer`.
- Keep reusable BACnet stack code separate from examples, tests, generated
  artifacts, and tools.
- Keep protocol encoding/decoding boundaries explicit.
- Do not add core dependencies on optional demo integrations.
- Do not import external BACnet implementation code without explicit license,
  provenance, and architecture review.

## Level C Risk

- BACnet protocol semantics
- network-facing behavior
- storage/NVS
- OTA
- security
- PlatformIO/build pipelines
- large refactors

## Tracking

- Use GitHub Project `ESP32 BACnet Stack` (#6) only when tracked workflow or
  project coordination is explicitly in scope.
- Release branches are optional before first release.
