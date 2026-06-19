# Raw GitHub Project Snapshot

[WARNING] This is a temporary raw GitHub Project snapshot for Serena context transfer. It is not curated long-term Serena memory.
[WARNING] Do not treat this file as source code provenance, implementation guidance, or a substitute for repository files and GitHub state.

Generated at: 2026-06-19T13:32:31Z
Repository: vitalyruhl/esp32-bacnet-stack
Project owner: vitalyruhl
Project number: 6
Project URL: https://github.com/users/vitalyruhl/projects/6
Item limit: 200
Issue number source: derived from project item content
Include closed issues: False

## Safety Notes

- This file is generated from GitHub metadata and issue text only.
- It must not contain secrets.
- It must not copy repository source code.
- Refresh it before relying on current planning state.

## Project Metadata

```json
{
  "closed": false,
  "fields": {
    "totalCount": 13
  },
  "id": "PVT_kwHOA_DIT84BbGt_",
  "items": {
    "totalCount": 14
  },
  "number": 6,
  "owner": {
    "login": "vitalyruhl",
    "type": "User"
  },
  "public": false,
  "readme": "",
  "shortDescription": "",
  "title": "ESP32 BACnet Stack",
  "url": "https://github.com/users/vitalyruhl/projects/6"
}
```

## Project Items

```json
{
  "items": [
    {
      "content": {
        "body": "Define and implement the first usable BACnet/IP server milestone for ESP32, including minimal device identity, object model scope, and validation criteria.",
        "number": 3,
        "repository": "vitalyruhl/esp32-bacnet-stack",
        "title": "Clean BACnet/IP server MVP",
        "type": "Issue",
        "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/3"
      },
      "id": "PVTI_lAHOA_DIT84BbGt_zgwQJO0",
      "repository": "https://github.com/vitalyruhl/esp32-bacnet-stack",
      "status": "Todo",
      "title": "Clean BACnet/IP server MVP"
    },
    {
      "content": {
        "body": "Define and implement the first usable BACnet/IP client milestone for ESP32, including discovery/read/write scope and validation criteria.",
        "number": 4,
        "repository": "vitalyruhl/esp32-bacnet-stack",
        "title": "Clean BACnet/IP client MVP",
        "type": "Issue",
        "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/4"
      },
      "id": "PVTI_lAHOA_DIT84BbGt_zgwQJPY",
      "repository": "https://github.com/vitalyruhl/esp32-bacnet-stack",
      "status": "Todo",
      "title": "Clean BACnet/IP client MVP"
    },
    {
      "content": {
        "body": "Evaluate upstream bacnet-stack architecture, license boundaries, source import strategy, and ESP32/Arduino adaptation requirements before importing code.",
        "number": 5,
        "repository": "vitalyruhl/esp32-bacnet-stack",
        "title": "Evaluate upstream bacnet-stack integration",
        "type": "Issue",
        "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/5"
      },
      "id": "PVTI_lAHOA_DIT84BbGt_zgwQJQE",
      "repository": "https://github.com/vitalyruhl/esp32-bacnet-stack",
      "status": "Todo",
      "title": "Evaluate upstream bacnet-stack integration"
    },
    {
      "content": {
        "body": "Extend the placeholder server demo into a basic BACnet/IP server example once the server MVP API is available.",
        "number": 6,
        "repository": "vitalyruhl/esp32-bacnet-stack",
        "title": "Add basic server example",
        "type": "Issue",
        "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/6"
      },
      "id": "PVTI_lAHOA_DIT84BbGt_zgwQJQc",
      "repository": "https://github.com/vitalyruhl/esp32-bacnet-stack",
      "status": "Todo",
      "title": "Add basic server example"
    },
    {
      "content": {
        "body": "Extend the placeholder client demo into a basic BACnet/IP client example once the client MVP API is available.",
        "number": 7,
        "repository": "vitalyruhl/esp32-bacnet-stack",
        "title": "Add basic client example",
        "type": "Issue",
        "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/7"
      },
      "id": "PVTI_lAHOA_DIT84BbGt_zgwQJRg",
      "repository": "https://github.com/vitalyruhl/esp32-bacnet-stack",
      "status": "Todo",
      "title": "Add basic client example"
    },
    {
      "content": {
        "body": "Keep PlatformIO CI and dependency maintenance reliable. Track Dependabot coverage limitations for PlatformIO dependencies.",
        "number": 8,
        "repository": "vitalyruhl/esp32-bacnet-stack",
        "title": "Add CI and dependency maintenance",
        "type": "Issue",
        "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/8"
      },
      "id": "PVTI_lAHOA_DIT84BbGt_zgwQJTA",
      "repository": "https://github.com/vitalyruhl/esp32-bacnet-stack",
      "status": "Todo",
      "title": "Add CI and dependency maintenance"
    },
    {
      "content": {
        "body": "After the first reviewed integration creates or updates main, configure branch protection or a ruleset with the PlatformIO workflow as a required status check if the account/repository plan supports it. Recheck GitHub Advanced Security and secret scanning availability; setup currently failed because GitHub reported Advanced Security has not been purchased.",
        "number": 9,
        "repository": "vitalyruhl/esp32-bacnet-stack",
        "title": "Configure branch protection and unavailable security features",
        "type": "Issue",
        "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/9"
      },
      "id": "PVTI_lAHOA_DIT84BbGt_zgwQJqE",
      "repository": "https://github.com/vitalyruhl/esp32-bacnet-stack",
      "status": "Todo",
      "title": "Configure branch protection and unavailable security features"
    },
    {
      "content": {
        "body": "## Scope\nInventory the BACnet/ESP32 reference landscape that should feed Serena memories.\n\n## User-provided research status\nThe following landscape is user-provided research until each project is verified from repository sources:\n\n| Project / Stack | ESP32 suitable? | BACnet/IP | BACnet MS/TP | Planning assessment |\n| --- | ---: | ---: | ---: | --- |\n| `bacnet-stack` / Steve Karg | indirectly | yes | yes | Mature open-source C stack; suitable as embedded porting basis, not Arduino-comfortable out of the box. |\n| `KaiDroste/bacnet-stack-esp32` | yes | probably yes | ESP32-port focus | ESP32 port/fork of the known bacnet-stack; interesting foundation, not finished Arduino package. |\n| `MGuerrero31416/BACnet-ESP32-Display` | yes | yes | yes | ESP32 firmware/example with BACnet/IP + MS/TP, objects, NVS, display; not primarily a reusable library. |\n| `chipkin/ESP32-BACnetServerExample` | yes | yes | no / not focus | PlatformIO example using commercial CAS BACnet Stack; useful for product lessons, not reusable open-source implementation material. |\n| `DoodzProg/ESP32-BMS-Gateway-Multi-Protocol` | ESP32-S3 | yes | no | Modbus TCP to BACnet/IP gateway with PlatformIO/Web-HMI; useful example, gateway-specific and not library architecture. |\n\n## Verified access so far\n`gh repo view` could access:\n- `bacnet-stack/bacnet-stack`\n- `KaiDroste/bacnet-stack-esp32`\n- `MGuerrero31416/BACnet-ESP32-Display`\n- `chipkin/ESP32-BACnetServerExample`\n- `DoodzProg/ESP32-BMS-Gateway-Multi-Protocol`\n\nAccess verification is not content verification. Source-derived facts still need direct repo inspection.\n\n## Likely affected repositories/files or Serena areas\n- Current library: `vitalyruhl/esp32-bacnet-stack`\n- Reference landscape: the five projects listed above\n- Serena areas: `shared/bacnet_reference_landscape`, `shared/esp32_bacnet_porting_candidates`, `shared/bacnet_transport_support_matrix`, `shared/bacnet_open_questions`\n\n## Dependencies\nNone.\n\n## Validation/check method\n- Verify project names, repository URLs, licensing/provenance, source layout, and BACnet/IP vs MS/TP support from each repository.\n- Mark inaccessible or ambiguous projects as unverified reference candidates.\n- Do not copy source code into memories.\n\n## Done criteria\n- Current library and each reference project has a concise role summary.\n- Reusable implementation references, ESP32 ports, firmware examples, commercial-stack-only references, and gateway-specific examples are separated.\n- BACnet/IP and MS/TP support is marked verified, uncertain, or not applicable.\n- Primary source/docs/test locations are listed with provenance references.\n- Unverified claims remain explicitly marked as user-provided or uncertain.\n\n## Parent planning issue\nhttps://github.com/vitalyruhl/esp32-bacnet-stack/issues/16\r\n",
        "number": 10,
        "repository": "vitalyruhl/esp32-bacnet-stack",
        "title": "Inventory BACnet reference landscape",
        "type": "Issue",
        "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/10"
      },
      "id": "PVTI_lAHOA_DIT84BbGt_zgwQv2g",
      "repository": "https://github.com/vitalyruhl/esp32-bacnet-stack",
      "status": "Todo",
      "title": "Inventory BACnet reference landscape"
    },
    {
      "content": {
        "body": "## Scope\nDefine the durable Serena memory structure for the BACnet/ESP32 reference landscape before writing memories.\n\n## Proposed memory categories and names\n- `shared/bacnet_reference_landscape`: current repo plus verified/unverified reference projects and roles.\n- `shared/esp32_bacnet_porting_candidates`: ESP32-oriented ports/forks and what must be verified before reuse.\n- `shared/bacnet_transport_support_matrix`: BACnet/IP vs MS/TP support, verification status, and source references.\n- `shared/reusable_stack_architecture_lessons`: stable reusable architecture/protocol lessons from open-source references.\n- `shared/non_reusable_commercial_references`: CAS/Chipkin and other commercial-stack observations that are not reusable implementation material.\n- `shared/bacnet_gateway_example_patterns`: gateway-specific lessons that should not drive core library architecture by default.\n- `shared/bacnet_open_questions`: uncertain facts, access gaps, licensing questions, and verification tasks.\n- `shared/bacnet_agent_start_points`: what future agents should inspect first and what not to assume.\n\nExisting memories that may be updated after approval:\n- `shared/project_overview`\n- `shared/architecture`\n- `shared/build_and_test_commands`\n- `core.md`\n\n## Boundaries\n- Separate reusable BACnet/C++/PlatformIO knowledge from `esp32-bacnet-stack` facts.\n- Separate open-source implementation references from commercial-stack-only observations.\n- Separate example firmware patterns from reusable library architecture.\n- Separate gateway-specific patterns from core library design.\n- Keep volatile facts out unless dated and marked volatile.\n\n## Dependencies\n- Inventory BACnet reference landscape\n\n## Validation/check method\n- Confirm every memory has one stable purpose.\n- Confirm every project-derived fact has provenance or uncertainty marking.\n- Confirm update rules say memories never override repository files, user instructions, `.github/AGENTS.md`, or `.github/agents/project.agent.md`.\n\n## Done criteria\n- Memory names and boundaries are approved.\n- Stable vs volatile facts are distinguished.\n- Verification/update rules are documented.\n- No memory is authorized for writing until the user approves the structure.\n\n## Parent planning issue\nhttps://github.com/vitalyruhl/esp32-bacnet-stack/issues/16\r\n",
        "number": 11,
        "repository": "vitalyruhl/esp32-bacnet-stack",
        "title": "Design Serena memory structure",
        "type": "Issue",
        "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/11"
      },
      "id": "PVTI_lAHOA_DIT84BbGt_zgwQv3I",
      "repository": "https://github.com/vitalyruhl/esp32-bacnet-stack",
      "status": "Todo",
      "title": "Design Serena memory structure"
    },
    {
      "content": {
        "body": "## Scope\nExtract reusable architecture/protocol context from the current library and verified reference landscape.\n\n## Context separation\n- Reusable architecture/protocol knowledge: protocol layers, BACnet/IP vs MS/TP concepts, encode/decode boundaries, object/device model concepts.\n- Open-source implementation references: `bacnet-stack/bacnet-stack` and ESP32 forks/ports after license/provenance verification.\n- ESP32/PlatformIO porting lessons: memory, networking, serial/RS-485, NVS, build constraints, Arduino ergonomics.\n- Example firmware patterns: display/NVS/object examples that inform demos but should not become default library architecture.\n- Commercial-stack-only observations: Chipkin/CAS behavior and PlatformIO setup patterns, not reusable code.\n- Gateway-specific patterns: Modbus-to-BACnet mapping/Web-HMI patterns, not core library assumptions.\n\n## Likely affected repositories/files or Serena areas\n- `vitalyruhl/esp32-bacnet-stack`: `README.md`, `docs/license-model.md`, `THIRD_PARTY_NOTICES.md`, `src/`, examples, tests, project profile.\n- Reference repos: inspect docs/source layout/license files first; then only focused architecture/protocol files.\n- Serena areas: `shared/reusable_stack_architecture_lessons`, `shared/bacnet_transport_support_matrix`, `shared/non_reusable_commercial_references`, `shared/bacnet_gateway_example_patterns`, `shared/bacnet_open_questions`.\n\n## Dependencies\n- Inventory BACnet reference landscape\n- Design Serena memory structure\n\n## Validation/check method\n- Summarize concepts and boundaries, not code.\n- Do not copy source code into memories.\n- Mark user-provided assessments as unverified until source-backed.\n- Include provenance references for each durable claim.\n\n## Done criteria\n- BACnet/IP and MS/TP support matrix is source-backed or marked uncertain.\n- Current library public API direction is summarized.\n- Protocol encode/decode boundaries and external import constraints are summarized.\n- Commercial and gateway-specific observations are explicitly separated from reusable library material.\n- Open questions are captured separately from facts.\n\n## Parent planning issue\nhttps://github.com/vitalyruhl/esp32-bacnet-stack/issues/16\r\n",
        "number": 12,
        "repository": "vitalyruhl/esp32-bacnet-stack",
        "title": "Extract architecture and protocol context",
        "type": "Issue",
        "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/12"
      },
      "id": "PVTI_lAHOA_DIT84BbGt_zgwQv3w",
      "repository": "https://github.com/vitalyruhl/esp32-bacnet-stack",
      "status": "Todo",
      "title": "Extract architecture and protocol context"
    },
    {
      "content": {
        "body": "## Scope\nExtract build, validation, workflow, and ESP32/PlatformIO porting context for future Serena memories.\n\n## Likely affected repositories/files or Serena areas\n- Current library: `.github/agents/project.agent.md`, `.github/AGENTS.md`, `.github/agents/workflow.agent.md`, `.github/workflows/`, `platformio.ini`, `README.md`, `test/README`, existing `shared/build_and_test_commands`.\n- Reference repos: PlatformIO/Arduino build docs, ESP32 board/env configuration, serial/RS-485/NVS/network setup notes where source-verified.\n- Serena areas: `shared/esp32_bacnet_porting_candidates`, `shared/bacnet_build_validation_workflow`, `shared/bacnet_agent_start_points`, `shared/bacnet_open_questions`.\n\n## Dependencies\n- Inventory BACnet reference landscape\n- Design Serena memory structure\n\n## Validation/check method\n- Use current project profile validation commands as authoritative for this repo.\n- Do not mix upstream/reference build commands into current repo validation defaults.\n- Capture reference build commands only as reference facts with provenance.\n- Do not run PlatformIO during this planning issue.\n\n## Done criteria\n- Current repo PlatformIO environments and validation commands are captured from the project profile.\n- Reference ESP32/PlatformIO porting lessons are separated from current repo commands.\n- CI, Dependabot, security, upload, and monitor constraints are summarized where useful.\n- Future agents know what validation to run, skip, or not assume.\n\n## Parent planning issue\nhttps://github.com/vitalyruhl/esp32-bacnet-stack/issues/16\r\n",
        "number": 13,
        "repository": "vitalyruhl/esp32-bacnet-stack",
        "title": "Extract build, validation, and workflow context",
        "type": "Issue",
        "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/13"
      },
      "id": "PVTI_lAHOA_DIT84BbGt_zgwQv4w",
      "repository": "https://github.com/vitalyruhl/esp32-bacnet-stack",
      "status": "Todo",
      "title": "Extract build, validation, and workflow context"
    },
    {
      "content": {
        "body": "## Scope\nWrite or update Serena memories only after the user approves the proposed memory structure.\n\n## Likely affected Serena areas\n- `.serena/memories/core.md`\n- `.serena/memories/shared/bacnet_reference_landscape.md`\n- `.serena/memories/shared/esp32_bacnet_porting_candidates.md`\n- `.serena/memories/shared/bacnet_transport_support_matrix.md`\n- `.serena/memories/shared/reusable_stack_architecture_lessons.md`\n- `.serena/memories/shared/non_reusable_commercial_references.md`\n- `.serena/memories/shared/bacnet_gateway_example_patterns.md`\n- `.serena/memories/shared/bacnet_open_questions.md`\n- `.serena/memories/shared/bacnet_agent_start_points.md`\n\n## Dependencies\n- Inventory BACnet reference landscape\n- Design Serena memory structure\n- Extract architecture and protocol context\n- Extract build, validation, and workflow context\n- Explicit user approval of the memory structure\n\n## Validation/check method\n- Do not include large code excerpts.\n- Include source repository/file references where useful.\n- Keep summaries concise, stable, and provenance-backed.\n- Keep secrets, credentials, tokens, private keys, and environment-specific sensitive data out of memory.\n- Keep commercial-stack observations separate from reusable open-source implementation notes.\n\n## Done criteria\n- Approved memories are written or updated.\n- Each memory states that repository files, user instructions, `.github/AGENTS.md`, and `project.agent.md` remain authoritative.\n- User-provided research is either source-verified or clearly marked unverified.\n- Memory index points future agents to the right memories without skipping required governance reads.\n\n## Parent planning issue\nhttps://github.com/vitalyruhl/esp32-bacnet-stack/issues/16\r\n",
        "number": 14,
        "repository": "vitalyruhl/esp32-bacnet-stack",
        "title": "Write Serena memories after approval",
        "type": "Issue",
        "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/14"
      },
      "id": "PVTI_lAHOA_DIT84BbGt_zgwQv5o",
      "repository": "https://github.com/vitalyruhl/esp32-bacnet-stack",
      "status": "Todo",
      "title": "Write Serena memories after approval"
    },
    {
      "content": {
        "body": "## Scope\nVerify the resulting Serena memories are useful, discoverable, and safe.\n\n## Likely affected repositories/files or Serena areas\n- `.serena/memories/core.md`\n- New/updated BACnet reference landscape memories\n- `.github/AGENTS.md`\n- `.github/agents/project.agent.md`\n\n## Dependencies\n- Write Serena memories after approval\n\n## Validation/check method\n- Verify memories are discoverable from the memory index.\n- Verify memories do not conflict with repository files or governance.\n- Verify future agents can start from memories without skipping mandatory direct governance reads.\n- Verify memories do not store secrets or large copied source blocks.\n- Verify commercial-stack-only and gateway-specific observations are clearly separated from reusable architecture guidance.\n- Verify unverified user-provided research remains marked unverified.\n\n## Done criteria\n- Memory index and memory names are coherent.\n- No memory contradicts `.github/AGENTS.md`, `project.agent.md`, or repository files.\n- Future-agent entry points are clear.\n- Remaining gaps or uncertainty are listed in the open-questions memory.\n- Reference landscape memories distinguish verified source facts from user-provided research.\n\n## Parent planning issue\nhttps://github.com/vitalyruhl/esp32-bacnet-stack/issues/16\r\n",
        "number": 15,
        "repository": "vitalyruhl/esp32-bacnet-stack",
        "title": "Audit Serena memory usefulness",
        "type": "Issue",
        "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/15"
      },
      "id": "PVTI_lAHOA_DIT84BbGt_zgwQv6U",
      "repository": "https://github.com/vitalyruhl/esp32-bacnet-stack",
      "status": "Todo",
      "title": "Audit Serena memory usefulness"
    },
    {
      "content": {
        "body": "## Goal\nPlan how to collect durable BACnet/ESP32 knowledge from the current library repository and a verified reference landscape into Serena project memories so future agents do not repeatedly re-read repositories for the same architecture, protocol, validation, and design context.\n\n## Scope\n- Current repository: `vitalyruhl/esp32-bacnet-stack`.\n- Reference landscape from user-provided research, to be source-verified before memory writing:\n  - `bacnet-stack/bacnet-stack`\n  - `KaiDroste/bacnet-stack-esp32`\n  - `MGuerrero31416/BACnet-ESP32-Display`\n  - `chipkin/ESP32-BACnetServerExample`\n  - `DoodzProg/ESP32-BMS-Gateway-Multi-Protocol`\n- Define memory categories, names, boundaries, verification rules, and update rules.\n- Plan extraction of stable architecture, protocol, ESP32 porting, validation, provenance, and open-question context.\n\n## Non-goals\n- Do not write Serena memories until the user approves the proposed memory structure.\n- Do not change product code, examples, CI, build metadata, or repository governance as part of this plan.\n- Do not copy source code into memories.\n- Do not treat commercial CAS/Chipkin code as reusable open-source implementation material.\n- Do not let memories override repository files, user instructions, `.github/AGENTS.md`, or `.github/agents/project.agent.md`.\n\n## Reference landscape handling\nThe reference table is user-provided research until source-verified. Repository access was verified with `gh repo view`, but content facts still require direct source/doc/license inspection.\n\n## Memory categories\n- BACnet reference landscape\n- ESP32 BACnet porting candidates\n- BACnet/IP vs MS/TP support matrix\n- Reusable stack architecture lessons\n- Open-source implementation references\n- Example firmware patterns\n- Non-reusable/commercial references\n- Non-library gateway-specific patterns\n- Open questions and verification tasks\n- Future-agent inspection entry points and non-assumptions\n\n## Proposed Serena memory names\n- `shared/bacnet_reference_landscape`\n- `shared/esp32_bacnet_porting_candidates`\n- `shared/bacnet_transport_support_matrix`\n- `shared/reusable_stack_architecture_lessons`\n- `shared/non_reusable_commercial_references`\n- `shared/bacnet_gateway_example_patterns`\n- `shared/bacnet_open_questions`\n- `shared/bacnet_agent_start_points`\n\nExisting memories that may be updated after approval:\n- `shared/project_overview`\n- `shared/architecture`\n- `shared/build_and_test_commands`\n- `core.md`\n\n## Information to capture per memory\n- Stable repository role and scope, with source references.\n- BACnet/IP vs MS/TP support, marked verified/uncertain/not applicable.\n- Reusable architecture/protocol concepts and encode/decode boundaries.\n- ESP32/PlatformIO porting lessons and constraints.\n- Current library facts and validation commands from the project profile.\n- Commercial-stack-only observations clearly separated from reusable implementation material.\n- Gateway-specific patterns clearly separated from core library architecture.\n- Open questions and unverified user-provided assessments.\n\n## What must not be stored in memory\n- Secrets, credentials, tokens, private keys, local-only sensitive data.\n- Large copied source-code blocks.\n- Volatile branch/CI status unless explicitly marked as current and dated.\n- Repository rules as an override of `.github/AGENTS.md` or `project.agent.md`.\n- Unverified upstream/reference assumptions presented as fact.\n- Commercial CAS/Chipkin source or implementation details as reusable open-source material.\n\n## Validation/audit approach\n- Verify project names and access with `gh repo view`.\n- Verify each project assessment from repository source/docs/license files before storing as fact.\n- Use direct file/repository references as provenance.\n- Keep memories concise and stable.\n- Confirm future agents can use memories as orientation while still performing mandatory direct governance reads.\n- Audit memory content against `.github/AGENTS.md`, `.github/agents/project.agent.md`, and source repository facts.\n\n## Risks\n- User-provided research can be stale or partially inaccurate until source-verified.\n- Reference repositories may change structure or license terms.\n- Example firmware or gateway projects may bias core library design if not clearly separated.\n- Commercial-stack examples may be useful operational references but cannot be treated as reusable implementation material.\n- Future agents may over-trust memories; every memory must state that repository files and governance remain authoritative.\n\n## Acceptance criteria\n- Parent and child issues exist and are added to GitHub Project `ESP32 BACnet Stack` (#6).\n- Reference landscape is captured without treating missing single ÔÇ£second repoÔÇØ as a blocker.\n- Proposed memory names and boundaries are documented.\n- User-provided research remains marked unverified until source-backed.\n- No Serena memory is written before user approval.\n- No product code, examples, CI, or build metadata is changed.\n- The plan includes a validation/audit path for memory usefulness and safety.\n\n## Child issues\n- [ ] Inventory BACnet reference landscape: https://github.com/vitalyruhl/esp32-bacnet-stack/issues/10\n- [ ] Design Serena memory structure: https://github.com/vitalyruhl/esp32-bacnet-stack/issues/11\n- [ ] Extract architecture and protocol context: https://github.com/vitalyruhl/esp32-bacnet-stack/issues/12\n- [ ] Extract build, validation, and workflow context: https://github.com/vitalyruhl/esp32-bacnet-stack/issues/13\n- [ ] Write Serena memories after approval: https://github.com/vitalyruhl/esp32-bacnet-stack/issues/14\n- [ ] Audit Serena memory usefulness: https://github.com/vitalyruhl/esp32-bacnet-stack/issues/15\n\n## Relationship fallback\nGitHub subissues were not created from this environment. Linked child issues plus this parent checklist are used as the supported fallback.\r\n",
        "number": 16,
        "repository": "vitalyruhl/esp32-bacnet-stack",
        "title": "Plan Serena knowledge capture for BACnet stack context",
        "type": "Issue",
        "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/16"
      },
      "id": "PVTI_lAHOA_DIT84BbGt_zgwQv6s",
      "repository": "https://github.com/vitalyruhl/esp32-bacnet-stack",
      "status": "Todo",
      "title": "Plan Serena knowledge capture for BACnet stack context"
    }
  ],
  "totalCount": 14
}
```

## Exported Issue Numbers

- #3
- #4
- #5
- #6
- #7
- #8
- #9
- #10
- #11
- #12
- #13
- #14
- #15
- #16

## Linked Issues

```json
[
  {
    "assignees": [],
    "body": "Define and implement the first usable BACnet/IP server milestone for ESP32, including minimal device identity, object model scope, and validation criteria.",
    "closedAt": null,
    "comments": [],
    "createdAt": "2026-06-19T10:41:02Z",
    "labels": [],
    "milestone": null,
    "number": 3,
    "state": "OPEN",
    "stateReason": "",
    "title": "Clean BACnet/IP server MVP",
    "updatedAt": "2026-06-19T10:41:02Z",
    "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/3"
  },
  {
    "assignees": [],
    "body": "Define and implement the first usable BACnet/IP client milestone for ESP32, including discovery/read/write scope and validation criteria.",
    "closedAt": null,
    "comments": [],
    "createdAt": "2026-06-19T10:41:04Z",
    "labels": [],
    "milestone": null,
    "number": 4,
    "state": "OPEN",
    "stateReason": "",
    "title": "Clean BACnet/IP client MVP",
    "updatedAt": "2026-06-19T10:41:04Z",
    "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/4"
  },
  {
    "assignees": [],
    "body": "Evaluate upstream bacnet-stack architecture, license boundaries, source import strategy, and ESP32/Arduino adaptation requirements before importing code.",
    "closedAt": null,
    "comments": [],
    "createdAt": "2026-06-19T10:41:07Z",
    "labels": [],
    "milestone": null,
    "number": 5,
    "state": "OPEN",
    "stateReason": "",
    "title": "Evaluate upstream bacnet-stack integration",
    "updatedAt": "2026-06-19T10:41:07Z",
    "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/5"
  },
  {
    "assignees": [],
    "body": "Extend the placeholder server demo into a basic BACnet/IP server example once the server MVP API is available.",
    "closedAt": null,
    "comments": [],
    "createdAt": "2026-06-19T10:41:10Z",
    "labels": [],
    "milestone": null,
    "number": 6,
    "state": "OPEN",
    "stateReason": "",
    "title": "Add basic server example",
    "updatedAt": "2026-06-19T10:41:10Z",
    "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/6"
  },
  {
    "assignees": [],
    "body": "Extend the placeholder client demo into a basic BACnet/IP client example once the client MVP API is available.",
    "closedAt": null,
    "comments": [],
    "createdAt": "2026-06-19T10:41:12Z",
    "labels": [],
    "milestone": null,
    "number": 7,
    "state": "OPEN",
    "stateReason": "",
    "title": "Add basic client example",
    "updatedAt": "2026-06-19T10:41:12Z",
    "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/7"
  },
  {
    "assignees": [],
    "body": "Keep PlatformIO CI and dependency maintenance reliable. Track Dependabot coverage limitations for PlatformIO dependencies.",
    "closedAt": null,
    "comments": [],
    "createdAt": "2026-06-19T10:41:15Z",
    "labels": [],
    "milestone": null,
    "number": 8,
    "state": "OPEN",
    "stateReason": "",
    "title": "Add CI and dependency maintenance",
    "updatedAt": "2026-06-19T10:41:15Z",
    "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/8"
  },
  {
    "assignees": [],
    "body": "After the first reviewed integration creates or updates main, configure branch protection or a ruleset with the PlatformIO workflow as a required status check if the account/repository plan supports it. Recheck GitHub Advanced Security and secret scanning availability; setup currently failed because GitHub reported Advanced Security has not been purchased.",
    "closedAt": null,
    "comments": [],
    "createdAt": "2026-06-19T10:42:42Z",
    "labels": [],
    "milestone": null,
    "number": 9,
    "state": "OPEN",
    "stateReason": "",
    "title": "Configure branch protection and unavailable security features",
    "updatedAt": "2026-06-19T10:42:42Z",
    "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/9"
  },
  {
    "assignees": [],
    "body": "## Scope\nInventory the BACnet/ESP32 reference landscape that should feed Serena memories.\n\n## User-provided research status\nThe following landscape is user-provided research until each project is verified from repository sources:\n\n| Project / Stack | ESP32 suitable? | BACnet/IP | BACnet MS/TP | Planning assessment |\n| --- | ---: | ---: | ---: | --- |\n| `bacnet-stack` / Steve Karg | indirectly | yes | yes | Mature open-source C stack; suitable as embedded porting basis, not Arduino-comfortable out of the box. |\n| `KaiDroste/bacnet-stack-esp32` | yes | probably yes | ESP32-port focus | ESP32 port/fork of the known bacnet-stack; interesting foundation, not finished Arduino package. |\n| `MGuerrero31416/BACnet-ESP32-Display` | yes | yes | yes | ESP32 firmware/example with BACnet/IP + MS/TP, objects, NVS, display; not primarily a reusable library. |\n| `chipkin/ESP32-BACnetServerExample` | yes | yes | no / not focus | PlatformIO example using commercial CAS BACnet Stack; useful for product lessons, not reusable open-source implementation material. |\n| `DoodzProg/ESP32-BMS-Gateway-Multi-Protocol` | ESP32-S3 | yes | no | Modbus TCP to BACnet/IP gateway with PlatformIO/Web-HMI; useful example, gateway-specific and not library architecture. |\n\n## Verified access so far\n`gh repo view` could access:\n- `bacnet-stack/bacnet-stack`\n- `KaiDroste/bacnet-stack-esp32`\n- `MGuerrero31416/BACnet-ESP32-Display`\n- `chipkin/ESP32-BACnetServerExample`\n- `DoodzProg/ESP32-BMS-Gateway-Multi-Protocol`\n\nAccess verification is not content verification. Source-derived facts still need direct repo inspection.\n\n## Likely affected repositories/files or Serena areas\n- Current library: `vitalyruhl/esp32-bacnet-stack`\n- Reference landscape: the five projects listed above\n- Serena areas: `shared/bacnet_reference_landscape`, `shared/esp32_bacnet_porting_candidates`, `shared/bacnet_transport_support_matrix`, `shared/bacnet_open_questions`\n\n## Dependencies\nNone.\n\n## Validation/check method\n- Verify project names, repository URLs, licensing/provenance, source layout, and BACnet/IP vs MS/TP support from each repository.\n- Mark inaccessible or ambiguous projects as unverified reference candidates.\n- Do not copy source code into memories.\n\n## Done criteria\n- Current library and each reference project has a concise role summary.\n- Reusable implementation references, ESP32 ports, firmware examples, commercial-stack-only references, and gateway-specific examples are separated.\n- BACnet/IP and MS/TP support is marked verified, uncertain, or not applicable.\n- Primary source/docs/test locations are listed with provenance references.\n- Unverified claims remain explicitly marked as user-provided or uncertain.\n\n## Parent planning issue\nhttps://github.com/vitalyruhl/esp32-bacnet-stack/issues/16\r\n",
    "closedAt": null,
    "comments": [],
    "createdAt": "2026-06-19T12:47:49Z",
    "labels": [],
    "milestone": null,
    "number": 10,
    "state": "OPEN",
    "stateReason": "",
    "title": "Inventory BACnet reference landscape",
    "updatedAt": "2026-06-19T12:51:22Z",
    "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/10"
  },
  {
    "assignees": [],
    "body": "## Scope\nDefine the durable Serena memory structure for the BACnet/ESP32 reference landscape before writing memories.\n\n## Proposed memory categories and names\n- `shared/bacnet_reference_landscape`: current repo plus verified/unverified reference projects and roles.\n- `shared/esp32_bacnet_porting_candidates`: ESP32-oriented ports/forks and what must be verified before reuse.\n- `shared/bacnet_transport_support_matrix`: BACnet/IP vs MS/TP support, verification status, and source references.\n- `shared/reusable_stack_architecture_lessons`: stable reusable architecture/protocol lessons from open-source references.\n- `shared/non_reusable_commercial_references`: CAS/Chipkin and other commercial-stack observations that are not reusable implementation material.\n- `shared/bacnet_gateway_example_patterns`: gateway-specific lessons that should not drive core library architecture by default.\n- `shared/bacnet_open_questions`: uncertain facts, access gaps, licensing questions, and verification tasks.\n- `shared/bacnet_agent_start_points`: what future agents should inspect first and what not to assume.\n\nExisting memories that may be updated after approval:\n- `shared/project_overview`\n- `shared/architecture`\n- `shared/build_and_test_commands`\n- `core.md`\n\n## Boundaries\n- Separate reusable BACnet/C++/PlatformIO knowledge from `esp32-bacnet-stack` facts.\n- Separate open-source implementation references from commercial-stack-only observations.\n- Separate example firmware patterns from reusable library architecture.\n- Separate gateway-specific patterns from core library design.\n- Keep volatile facts out unless dated and marked volatile.\n\n## Dependencies\n- Inventory BACnet reference landscape\n\n## Validation/check method\n- Confirm every memory has one stable purpose.\n- Confirm every project-derived fact has provenance or uncertainty marking.\n- Confirm update rules say memories never override repository files, user instructions, `.github/AGENTS.md`, or `.github/agents/project.agent.md`.\n\n## Done criteria\n- Memory names and boundaries are approved.\n- Stable vs volatile facts are distinguished.\n- Verification/update rules are documented.\n- No memory is authorized for writing until the user approves the structure.\n\n## Parent planning issue\nhttps://github.com/vitalyruhl/esp32-bacnet-stack/issues/16\r\n",
    "closedAt": null,
    "comments": [],
    "createdAt": "2026-06-19T12:47:52Z",
    "labels": [],
    "milestone": null,
    "number": 11,
    "state": "OPEN",
    "stateReason": "",
    "title": "Design Serena memory structure",
    "updatedAt": "2026-06-19T12:51:23Z",
    "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/11"
  },
  {
    "assignees": [],
    "body": "## Scope\nExtract reusable architecture/protocol context from the current library and verified reference landscape.\n\n## Context separation\n- Reusable architecture/protocol knowledge: protocol layers, BACnet/IP vs MS/TP concepts, encode/decode boundaries, object/device model concepts.\n- Open-source implementation references: `bacnet-stack/bacnet-stack` and ESP32 forks/ports after license/provenance verification.\n- ESP32/PlatformIO porting lessons: memory, networking, serial/RS-485, NVS, build constraints, Arduino ergonomics.\n- Example firmware patterns: display/NVS/object examples that inform demos but should not become default library architecture.\n- Commercial-stack-only observations: Chipkin/CAS behavior and PlatformIO setup patterns, not reusable code.\n- Gateway-specific patterns: Modbus-to-BACnet mapping/Web-HMI patterns, not core library assumptions.\n\n## Likely affected repositories/files or Serena areas\n- `vitalyruhl/esp32-bacnet-stack`: `README.md`, `docs/license-model.md`, `THIRD_PARTY_NOTICES.md`, `src/`, examples, tests, project profile.\n- Reference repos: inspect docs/source layout/license files first; then only focused architecture/protocol files.\n- Serena areas: `shared/reusable_stack_architecture_lessons`, `shared/bacnet_transport_support_matrix`, `shared/non_reusable_commercial_references`, `shared/bacnet_gateway_example_patterns`, `shared/bacnet_open_questions`.\n\n## Dependencies\n- Inventory BACnet reference landscape\n- Design Serena memory structure\n\n## Validation/check method\n- Summarize concepts and boundaries, not code.\n- Do not copy source code into memories.\n- Mark user-provided assessments as unverified until source-backed.\n- Include provenance references for each durable claim.\n\n## Done criteria\n- BACnet/IP and MS/TP support matrix is source-backed or marked uncertain.\n- Current library public API direction is summarized.\n- Protocol encode/decode boundaries and external import constraints are summarized.\n- Commercial and gateway-specific observations are explicitly separated from reusable library material.\n- Open questions are captured separately from facts.\n\n## Parent planning issue\nhttps://github.com/vitalyruhl/esp32-bacnet-stack/issues/16\r\n",
    "closedAt": null,
    "comments": [],
    "createdAt": "2026-06-19T12:47:54Z",
    "labels": [],
    "milestone": null,
    "number": 12,
    "state": "OPEN",
    "stateReason": "",
    "title": "Extract architecture and protocol context",
    "updatedAt": "2026-06-19T12:51:24Z",
    "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/12"
  },
  {
    "assignees": [],
    "body": "## Scope\nExtract build, validation, workflow, and ESP32/PlatformIO porting context for future Serena memories.\n\n## Likely affected repositories/files or Serena areas\n- Current library: `.github/agents/project.agent.md`, `.github/AGENTS.md`, `.github/agents/workflow.agent.md`, `.github/workflows/`, `platformio.ini`, `README.md`, `test/README`, existing `shared/build_and_test_commands`.\n- Reference repos: PlatformIO/Arduino build docs, ESP32 board/env configuration, serial/RS-485/NVS/network setup notes where source-verified.\n- Serena areas: `shared/esp32_bacnet_porting_candidates`, `shared/bacnet_build_validation_workflow`, `shared/bacnet_agent_start_points`, `shared/bacnet_open_questions`.\n\n## Dependencies\n- Inventory BACnet reference landscape\n- Design Serena memory structure\n\n## Validation/check method\n- Use current project profile validation commands as authoritative for this repo.\n- Do not mix upstream/reference build commands into current repo validation defaults.\n- Capture reference build commands only as reference facts with provenance.\n- Do not run PlatformIO during this planning issue.\n\n## Done criteria\n- Current repo PlatformIO environments and validation commands are captured from the project profile.\n- Reference ESP32/PlatformIO porting lessons are separated from current repo commands.\n- CI, Dependabot, security, upload, and monitor constraints are summarized where useful.\n- Future agents know what validation to run, skip, or not assume.\n\n## Parent planning issue\nhttps://github.com/vitalyruhl/esp32-bacnet-stack/issues/16\r\n",
    "closedAt": null,
    "comments": [],
    "createdAt": "2026-06-19T12:47:57Z",
    "labels": [],
    "milestone": null,
    "number": 13,
    "state": "OPEN",
    "stateReason": "",
    "title": "Extract build, validation, and workflow context",
    "updatedAt": "2026-06-19T12:51:25Z",
    "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/13"
  },
  {
    "assignees": [],
    "body": "## Scope\nWrite or update Serena memories only after the user approves the proposed memory structure.\n\n## Likely affected Serena areas\n- `.serena/memories/core.md`\n- `.serena/memories/shared/bacnet_reference_landscape.md`\n- `.serena/memories/shared/esp32_bacnet_porting_candidates.md`\n- `.serena/memories/shared/bacnet_transport_support_matrix.md`\n- `.serena/memories/shared/reusable_stack_architecture_lessons.md`\n- `.serena/memories/shared/non_reusable_commercial_references.md`\n- `.serena/memories/shared/bacnet_gateway_example_patterns.md`\n- `.serena/memories/shared/bacnet_open_questions.md`\n- `.serena/memories/shared/bacnet_agent_start_points.md`\n\n## Dependencies\n- Inventory BACnet reference landscape\n- Design Serena memory structure\n- Extract architecture and protocol context\n- Extract build, validation, and workflow context\n- Explicit user approval of the memory structure\n\n## Validation/check method\n- Do not include large code excerpts.\n- Include source repository/file references where useful.\n- Keep summaries concise, stable, and provenance-backed.\n- Keep secrets, credentials, tokens, private keys, and environment-specific sensitive data out of memory.\n- Keep commercial-stack observations separate from reusable open-source implementation notes.\n\n## Done criteria\n- Approved memories are written or updated.\n- Each memory states that repository files, user instructions, `.github/AGENTS.md`, and `project.agent.md` remain authoritative.\n- User-provided research is either source-verified or clearly marked unverified.\n- Memory index points future agents to the right memories without skipping required governance reads.\n\n## Parent planning issue\nhttps://github.com/vitalyruhl/esp32-bacnet-stack/issues/16\r\n",
    "closedAt": null,
    "comments": [],
    "createdAt": "2026-06-19T12:47:59Z",
    "labels": [],
    "milestone": null,
    "number": 14,
    "state": "OPEN",
    "stateReason": "",
    "title": "Write Serena memories after approval",
    "updatedAt": "2026-06-19T12:51:26Z",
    "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/14"
  },
  {
    "assignees": [],
    "body": "## Scope\nVerify the resulting Serena memories are useful, discoverable, and safe.\n\n## Likely affected repositories/files or Serena areas\n- `.serena/memories/core.md`\n- New/updated BACnet reference landscape memories\n- `.github/AGENTS.md`\n- `.github/agents/project.agent.md`\n\n## Dependencies\n- Write Serena memories after approval\n\n## Validation/check method\n- Verify memories are discoverable from the memory index.\n- Verify memories do not conflict with repository files or governance.\n- Verify future agents can start from memories without skipping mandatory direct governance reads.\n- Verify memories do not store secrets or large copied source blocks.\n- Verify commercial-stack-only and gateway-specific observations are clearly separated from reusable architecture guidance.\n- Verify unverified user-provided research remains marked unverified.\n\n## Done criteria\n- Memory index and memory names are coherent.\n- No memory contradicts `.github/AGENTS.md`, `project.agent.md`, or repository files.\n- Future-agent entry points are clear.\n- Remaining gaps or uncertainty are listed in the open-questions memory.\n- Reference landscape memories distinguish verified source facts from user-provided research.\n\n## Parent planning issue\nhttps://github.com/vitalyruhl/esp32-bacnet-stack/issues/16\r\n",
    "closedAt": null,
    "comments": [],
    "createdAt": "2026-06-19T12:48:02Z",
    "labels": [],
    "milestone": null,
    "number": 15,
    "state": "OPEN",
    "stateReason": "",
    "title": "Audit Serena memory usefulness",
    "updatedAt": "2026-06-19T12:51:27Z",
    "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/15"
  },
  {
    "assignees": [],
    "body": "## Goal\nPlan how to collect durable BACnet/ESP32 knowledge from the current library repository and a verified reference landscape into Serena project memories so future agents do not repeatedly re-read repositories for the same architecture, protocol, validation, and design context.\n\n## Scope\n- Current repository: `vitalyruhl/esp32-bacnet-stack`.\n- Reference landscape from user-provided research, to be source-verified before memory writing:\n  - `bacnet-stack/bacnet-stack`\n  - `KaiDroste/bacnet-stack-esp32`\n  - `MGuerrero31416/BACnet-ESP32-Display`\n  - `chipkin/ESP32-BACnetServerExample`\n  - `DoodzProg/ESP32-BMS-Gateway-Multi-Protocol`\n- Define memory categories, names, boundaries, verification rules, and update rules.\n- Plan extraction of stable architecture, protocol, ESP32 porting, validation, provenance, and open-question context.\n\n## Non-goals\n- Do not write Serena memories until the user approves the proposed memory structure.\n- Do not change product code, examples, CI, build metadata, or repository governance as part of this plan.\n- Do not copy source code into memories.\n- Do not treat commercial CAS/Chipkin code as reusable open-source implementation material.\n- Do not let memories override repository files, user instructions, `.github/AGENTS.md`, or `.github/agents/project.agent.md`.\n\n## Reference landscape handling\nThe reference table is user-provided research until source-verified. Repository access was verified with `gh repo view`, but content facts still require direct source/doc/license inspection.\n\n## Memory categories\n- BACnet reference landscape\n- ESP32 BACnet porting candidates\n- BACnet/IP vs MS/TP support matrix\n- Reusable stack architecture lessons\n- Open-source implementation references\n- Example firmware patterns\n- Non-reusable/commercial references\n- Non-library gateway-specific patterns\n- Open questions and verification tasks\n- Future-agent inspection entry points and non-assumptions\n\n## Proposed Serena memory names\n- `shared/bacnet_reference_landscape`\n- `shared/esp32_bacnet_porting_candidates`\n- `shared/bacnet_transport_support_matrix`\n- `shared/reusable_stack_architecture_lessons`\n- `shared/non_reusable_commercial_references`\n- `shared/bacnet_gateway_example_patterns`\n- `shared/bacnet_open_questions`\n- `shared/bacnet_agent_start_points`\n\nExisting memories that may be updated after approval:\n- `shared/project_overview`\n- `shared/architecture`\n- `shared/build_and_test_commands`\n- `core.md`\n\n## Information to capture per memory\n- Stable repository role and scope, with source references.\n- BACnet/IP vs MS/TP support, marked verified/uncertain/not applicable.\n- Reusable architecture/protocol concepts and encode/decode boundaries.\n- ESP32/PlatformIO porting lessons and constraints.\n- Current library facts and validation commands from the project profile.\n- Commercial-stack-only observations clearly separated from reusable implementation material.\n- Gateway-specific patterns clearly separated from core library architecture.\n- Open questions and unverified user-provided assessments.\n\n## What must not be stored in memory\n- Secrets, credentials, tokens, private keys, local-only sensitive data.\n- Large copied source-code blocks.\n- Volatile branch/CI status unless explicitly marked as current and dated.\n- Repository rules as an override of `.github/AGENTS.md` or `project.agent.md`.\n- Unverified upstream/reference assumptions presented as fact.\n- Commercial CAS/Chipkin source or implementation details as reusable open-source material.\n\n## Validation/audit approach\n- Verify project names and access with `gh repo view`.\n- Verify each project assessment from repository source/docs/license files before storing as fact.\n- Use direct file/repository references as provenance.\n- Keep memories concise and stable.\n- Confirm future agents can use memories as orientation while still performing mandatory direct governance reads.\n- Audit memory content against `.github/AGENTS.md`, `.github/agents/project.agent.md`, and source repository facts.\n\n## Risks\n- User-provided research can be stale or partially inaccurate until source-verified.\n- Reference repositories may change structure or license terms.\n- Example firmware or gateway projects may bias core library design if not clearly separated.\n- Commercial-stack examples may be useful operational references but cannot be treated as reusable implementation material.\n- Future agents may over-trust memories; every memory must state that repository files and governance remain authoritative.\n\n## Acceptance criteria\n- Parent and child issues exist and are added to GitHub Project `ESP32 BACnet Stack` (#6).\n- Reference landscape is captured without treating missing single ÔÇ£second repoÔÇØ as a blocker.\n- Proposed memory names and boundaries are documented.\n- User-provided research remains marked unverified until source-backed.\n- No Serena memory is written before user approval.\n- No product code, examples, CI, or build metadata is changed.\n- The plan includes a validation/audit path for memory usefulness and safety.\n\n## Child issues\n- [ ] Inventory BACnet reference landscape: https://github.com/vitalyruhl/esp32-bacnet-stack/issues/10\n- [ ] Design Serena memory structure: https://github.com/vitalyruhl/esp32-bacnet-stack/issues/11\n- [ ] Extract architecture and protocol context: https://github.com/vitalyruhl/esp32-bacnet-stack/issues/12\n- [ ] Extract build, validation, and workflow context: https://github.com/vitalyruhl/esp32-bacnet-stack/issues/13\n- [ ] Write Serena memories after approval: https://github.com/vitalyruhl/esp32-bacnet-stack/issues/14\n- [ ] Audit Serena memory usefulness: https://github.com/vitalyruhl/esp32-bacnet-stack/issues/15\n\n## Relationship fallback\nGitHub subissues were not created from this environment. Linked child issues plus this parent checklist are used as the supported fallback.\r\n",
    "closedAt": null,
    "comments": [],
    "createdAt": "2026-06-19T12:48:05Z",
    "labels": [],
    "milestone": null,
    "number": 16,
    "state": "OPEN",
    "stateReason": "",
    "title": "Plan Serena knowledge capture for BACnet stack context",
    "updatedAt": "2026-06-19T12:51:28Z",
    "url": "https://github.com/vitalyruhl/esp32-bacnet-stack/issues/16"
  }
]
```

