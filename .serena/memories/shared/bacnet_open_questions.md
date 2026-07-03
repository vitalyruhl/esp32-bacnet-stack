# BACnet Open Questions

- Status: uncertain facts and follow-up tasks.
- This memory is orientation only and does not replace repository files, user instructions, `.github/AGENTS.md`, or `.github/agents/project.agent.md`.

## Current Repo Gaps

- MS/TP is planned later in the current repo, but there is no source-backed current implementation here.
- Future memory updates should keep current-repo transport support aligned with repository files and not with reference-repo capabilities.

## Reference Uncertainties

- `KaiDroste/bacnet-stack-esp32` maintenance freshness and upgrade path remain unverified.
- The exact supported scope of upstream `bacnet-stack` ESP32 ports should be rechecked if future work depends on those ports directly.
- The exact read/write coverage of `MGuerrero31416/BACnet-ESP32-Display` across all transports was not fully audited for memory writing.
- BACnet Secure Connect support on the listed ESP32 references was not verified and must not be assumed.

## Usage Rule

- Keep unresolved facts here until they are backed by repository sources.

## Primary Sources

- Reference-repo README and port/source files summarized in `bacnet_reference_landscape.md`
