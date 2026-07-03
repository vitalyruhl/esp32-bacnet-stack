# BACnet Transport Support Matrix

- Status: mixed verified reference summary.
- This memory is orientation only and does not replace repository files, user instructions, `.github/AGENTS.md`, or `.github/agents/project.agent.md`.

## Current Repo

- `esp32-bacnet-stack`: BACnet/IP is the first implementation target.
- `esp32-bacnet-stack`: MS/TP is planned later and must be treated as not implemented unless repository files say otherwise.

## Reference Matrix

- `bacnet-stack/bacnet-stack`
  - BACnet/IP: verified
  - MS/TP: verified
  - Notes: upstream protocol-stack baseline
- `KaiDroste/bacnet-stack-esp32`
  - BACnet/IP: verified
  - MS/TP: verified
  - Notes: ESP32-focused fork/port; maintenance freshness unverified
- `MGuerrero31416/BACnet-ESP32-Display`
  - BACnet/IP: verified
  - MS/TP: verified
  - Notes: dual-mode ESP32 firmware example
- `chipkin/ESP32-BACnetServerExample`
  - BACnet/IP: verified
  - MS/TP: not applicable / not evidenced
  - Notes: proprietary-stack BACnet/IP example only
- `DoodzProg/ESP32-BMS-Gateway-Multi-Protocol`
  - BACnet/IP: verified
  - MS/TP: not applicable / not evidenced
  - Notes: gateway product pattern, not a generic BACnet stack

## Interpretation Rules

- Do not treat BACnet/IP and MS/TP as interchangeable.
- Do not infer current-repo MS/TP support from upstream/reference repos.
- Mark uncertain or inaccessible transport claims in `bacnet_open_questions` instead of promoting them to facts.

## Primary Sources

- Current repo: `README.md`, `.github/agents/project.agent.md`, `.serena/memories/shared/architecture.md`
- Upstream/reference repos: their `README.md` files and port/source directories noted in `bacnet_reference_landscape.md`
