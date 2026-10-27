# Digit — ROADMAP and Development Milestones

**Baseline:** 2026-10-27 repository `main`. This is a continuity plan, not automatic authorization to execute milestones. Digit's established Core structure and the shared STN-LABZ ABI 1.5 are the starting point. Core modifications require direct consent from the Director of the Digit Project.

## AI Assistance — Authority and Operational Boundaries

**Human Authority**

All AI assistants participating in the Digit Project operate under the authority and direction of the Project Director.

**AI assistance does not constitute decision-making authority, operational command, or permission to act independently.**

The Project Director retains exclusive authority over project direction, development priorities, architectural decisions, implementation approval, qualification acceptance, deployment, and operational changes.

**Mandatory AI Conduct**
1. No Autonomous Authorization: AI assistants shall not initiate, expand, redirect, or execute development activities without explicit authorization from the Project Director.
2. No Implied Consent: Access to source code, repositories, documentation, development tools, or previous conversations does not constitute permission to modify anything.
3. No Unauthorized Core Modifications: Protected Digit Core components and shared ABI contracts shall not be modified without direct, specific authorization. Approval of one modification does not authorize additional modifications.
4. No Unauthorized Workflows: AI assistants shall not initiate background work, autonomous agents, repository operations, deployment workflows, or external development sessions without the Director's authorization.
5. No Architectural Substitution: AI assistants shall preserve the established Core, module, controller, model, view, and transport responsibilities. Alternative architectures may be proposed for consideration but shall not be implemented without approval.
6. No Scope Expansion: An authorized task shall remain within its explicitly approved scope. Discovering an unrelated defect does not authorize correcting it.
7. Evidence Before Claims: AI assistants shall distinguish proposed, committed, built, verified, qualified, activated, and operator-accepted states. No success, qualification, or acceptance shall be fabricated or inferred.
8. Mandatory Documentation: Authorized development changes shall be accurately recorded in the CHANGELOG and ROADMAP, preserving decisions, outstanding work, qualification evidence, and operational boundaries.
9. Human-Controlled Progression: Completion of a task, passing tests, or reaching a milestone does not authorize proceeding to another task or milestone.
10. Stop and Report: If an action requires authorization that has not been granted, or an instruction conflicts with established project authority or governing documentation, the AI shall stop the affected action, report the issue, and await the Director's decision.

**Authority Limitation**

An AI assistant has no independent authority over the Digit Project.

Recommendations are not decisions. Proposals are not approvals. Tests are not acceptance. Repository access is not authorization.

Approval of this ROADMAP establishes its governance and development direction. It does not grant blanket permission to execute its milestones.

**The Project Director directs. AI assistants assist. Human authority is final.**
---

## Architectural contract

- **Core:** routing and management. A dumb router resolves a registered module destination and dispatches; it does not interpret requests, implement model logic, or format reports.
- **Module controller:** owns routing *inside* its module.
- **Module model:** owns its functional work and data interactions.
- **Module view:** optional, used only when special output/report rendering is needed.
- **Transport (IRC, Slack, Ollama):** its protocol and communication concerns only; no application-level capability selection or intelligence.
- **ABI:** the established Core-mediated registration, invocation, and communication interface. Modules must not directly bind to each other.
- **Root-cause standard:** competency is demonstrated; root causes are corrected. Evidence, not expedient workarounds, determines the component to change.

## Milestone 0 — Continuity baseline

**State: Continuity baseline documented; Director's 2026-10-27 Roadmap approval recorded below. Acceptance of individual engineering milestones remains separate.**

- Maintain `docs/CHANGELOG.md` (what actually changed) and this `docs/ROADMAP.md` (gates, remaining work, and decisions).
- Keep module version, Core revision, source commit, build evidence, qualification, installation, runtime results, and operator acceptance distinct.
- Identify any historical documentation links with absent backing files; do not substitute invented qualification receipts.
- **Exit gate:** Director reviews and accepts the continuity baseline.

## Milestone 1 — Core Router and module controller handoff

**State: Core clean build and full configured test suite PASSED on 2026-10-27; revised Core installed and reported operational by Director. IRC 1.1.0 and Ollama 1.0.4 module activations also confirmed. Independent final acceptance for each broader transport requirement remains separate.**

- Operational evidence supplied on 2026-10-27: IRC 1.0.11 activated, while last observed Ollama activation was 1.0.3; zero replies to nine test messages. IRC source identity subsequently corrected to doctrine-compliant 1.1.0. Ollama source is 1.0.4, but its deployment has not been established.
- **Subsequent operator receipts:** IRC 1.1.0 AUTO_ACTIVE at 16:32:15 UTC; Ollama 1.0.4 AUTO_ACTIVE at 16:43:14 UTC. `digicat: Hello` and `digicat: /chat Hello` both returned replies. `digicat: /unknown test` produced ABI `NOT_FOUND` (result=5) in IRC logs. These supersede the earlier unverified-deployment state without erasing the failure history.
- **Updated evidence:** Director reported `make clean && make && make test` PASS on current Core checkout and confirmed installed/operational status. Earlier individual suites passed: 20/20 static source contracts (not behavioral), 20/20 executable Root/policy, 20/20 hotload integration, 23/23 concurrency, 13/13 self-unregister, 15/15 in-flight replacement; 111/111 combined checks of mixed types. IRC 1.1.0 and Ollama 1.0.4 qualification/activation receipts were also supplied (20/20 and 10/10 respectively, with negative tests). Remaining: installed-artifact provenance as needed; broader identity/authority, bounded route error delivery, and explicit Director acceptance.

- Existing ABI 1.5 `register_command`, `unregister_command`, and `invoke_service` are used for controller registration and Core dispatch.
- New Core-owned ingress contract: `digit.router.dispatch`.
- Core selects a registered destination; the module controller selects its own operation.
- IRC uses Core ingress, not direct `digit.llm.generate`.
- Ollama provides an initial registered `chat` controller.
- **Open inspection:** route naming/collisions; exact command parser behavior; source/authority integrity; response and argument bounds; safe unload and registration withdrawal; error semantics; no hardcoded per-module Core logic.
- **Exit gate:** review full-file source and external ABI contract; compile and run qualified negative/positive tests; independent Core, IRC, Ollama verification; operator acceptance.

### Installation-integrity gate (2026-10-27)

- The Ollama installed artifact did not match its built artifact and was owned by `root:root` instead of `digit:digit` in operator-provided evidence.
- IRC and Ollama Makefiles now enforce user/group, compare built and installed bytes, and stage/rename binaries within the watched directory. Source committed; IRC 1.1.0 and Ollama 1.0.4 activation confirmed by operator. Post-install byte-hash and owner receipts were not supplied.
- Capture `sha256sum`, `stat`, and module activation receipts; then re-run routed IRC tests. Do not infer an installed revision from GitHub source.

### Protected Core hotload correction (2026-10-27)

- Source correction committed: incumbent shared object retained until replacement activation succeeds; rollback attempts reactivate incumbent on candidate activation failure.
- New and updated module qualification events record test-count and negative-test evidence in `modules.log`.
- **Updated gate evidence:** Core behavioral and hotload suites subsequently passed, including candidate activation-failure rollback, loader ownership, stale-route removal and in-flight replacement checks. Director reported consolidated clean-build/full-suite PASS, followed by successful Core installation and operation. A zero-warning `-Werror` build and all possible live module stop/fault scenarios have not been separately evidenced.
- Director controls activation of the revised Core.

### Concurrent routing and withdrawal verification (2026-10-27)

- Threaded service and command tests now use the actual Core host to verify that unregister waits for active callbacks, removes routes and permits safe slot reuse.
- `make test-concurrency` is wired into `make test`; Director subsequently reported 23/23 PASS, followed by consolidated `make clean && make && make test` PASS.
- Self-unregister protection subsequently passed 13/13 and in-flight hotload replacement 15/15; broader module-defined stop behavior under arbitrary live workloads remains separate.

### Hotload integration test audit correction (2026-10-27)

- Operator observed 19/20 integration results. Actual replacement, rollback, and loader assertions passed; only an audit test falsely expected suppressed success-detail text.
- Assertion now requires two emitted `HOTLOAD_ACTIVE` events. Director subsequently confirmed 20/20 PASS; the preceding 19/20 report is retained as failure history.

### Core hotload executable integration tests (2026-10-27)

- Four real `.so` candidate fixtures and 20 executable integration assertions now exercise Core's actual qualification rejection, candidate activation failure, incumbent rollback, successful replacement and log evidence.
- `make test` runs these alongside Root/policy and source-contract tests. Director subsequently confirmed hotload 20/20 PASS, concurrent routing 23/23 PASS, and in-flight hotload 15/15 PASS, plus consolidated clean-build/full-suite PASS.

### Executable Core unit behavioral tests (2026-10-27)

- `make test` compiles/runs 20 C unit behavior cases for Root and policy evaluation (positive and negative paths); Director subsequently confirmed 20/20 PASS and later consolidated test PASS.
- Full module lifecycle/hotload regression and fault injection still required. Unit counts cannot substitute for all applicable Core qualification requirements.

### Core test infrastructure restoration (2026-10-27)

- Root `make test` now builds Core and invokes 20 source-contract regression checks; `docs/CORE_QUALIFICATION.md` restored.
- **Classification preserved:** the 20 source-contract checks are static and do not count as the mandatory behavioral Core minimum. Separate 20/20 executable Root/policy behavior and further executable integration suites subsequently passed, including a consolidated clean build/test receipt.

## Milestone 2 — Transport neutrality and end-to-end delivery

**State: Planned; not authorized by prior source acceptance.**

- Verify inbound channel/private messaging carries the correct origin, authenticated identity when available, and return destination.
- Verify unknown/unavailable routes fail deterministically without fabricated responses.
- Verify ordinary channel traffic is ignored as appropriate, and addressed messages reach registered module controllers.
- Ensure IRC/Slack/Ollama can be changed or absent without requiring unrelated capability modules to change.
- Investigate existing transport-send bindings in Core (currently default IRC-oriented) before proposing any transport selection changes.
- **Exit gate:** evidence-backed ingress → Core → controller → model → Core → originating transport tests, including negative cases.

## Milestone 3 — Module-local MVC contract

**State: Planned.**

- Validate a reference Digit module against `example.zip`, `MODULE_CREATION_GUIDE.md`, `20261016.0_AI_MODULE_CREATION.md`, and `AGENTS.md` before module work.
- Define minimal controller registration and internal action dispatch; models work within their module; views remain optional.
- A new module operation must not require edits to Core routing tables or IRC.
- Preserve separate gates for source, build, qualification (including required negative tests), activation, end-to-end verification, and operator acceptance.
- **Exit gate:** demonstrable module-owned routing and safe start/stop/replacement without cross-module shortcuts.

## Milestone 4 — Operational module integration

**State: S2 Sentinel collection operational and operator-demonstrated in IRC; full S2 investigation restoration remains open. Research and Publish integrations remain deferred and separately authorized.**

- S2 `modules/s2` v1.0.3 is now installed/operational per Director: Sentinel HTTPS collection, durable source-ID register, four-hour polling, `/s2` status with readable UTC timestamp, and a change-gated scheduled IRC synopsis implementation. Research and Publish remain separate work; do not assume unimplemented routes exist.
- Passive Research or S2 work must not depend on someone typing an IRC command.
- Optional views are appropriate for detailed Markdown reports and publishing formats.
- Report generation, destination selection, and transport delivery remain distinct responsibilities.
- **Exit gate:** per-module tests and operator-approved acceptance criteria, without transport-specific feature patches.

## Milestone 5 — Additional transports

**State: Deferred pending separate authorization.**

- Rebuild Slack as an independent communications transport when requested.
- Apply the same Core handoff contract and transport-only responsibility as IRC.
- **Exit gate:** independent transport qualification and identical module behavior across supported interfaces.

## Required engineering sequence

`Requirement → Inspection → Evidence → Proposal → Director approval (when required) → Full-file implementation → Build → Verification → Qualification → Activation → End-to-end operator acceptance → Next explicit instruction`

### Open decisions

- Whether the 512-byte ABI command-argument field is sufficient for general routed messages.
- How route metadata carries origin, return destination, and authenticated authority without trusting unverified transport text.
- Whether default `chat` routing is appropriate beyond the initial compatibility path.
- Whether existing Core send-message functions require generalization to avoid IRC-first output behavior.
- How to validate unload safety, concurrency, and failures before expanding the contract.

These are review items, not permission to enlarge the ABI or rewrite Core.

## Core self-unregistration guard

2026-10-27: Director-authorized Core fix: module callback self-unregistration now fails immediately instead of deadlocking on its own in-flight counter. `make test-reentrant` is included in `make test`. Originally described as 12 assertions; actual corrected harness reports 13/13 PASS. Director also reported consolidated clean-build/full-suite PASS after this correction.

### Self-unregister deadlock correction

2026-10-27: Director-authorized Core fix: module callback self-unregistration now fails immediately instead of deadlocking on its own in-flight counter. `make test-reentrant` is included in `make test`. Originally described as 12 assertions; actual corrected harness reports 13/13 PASS. Director also reported consolidated clean-build/full-suite PASS after this correction.

## Concurrent hotload with an in-flight callback

2026-10-27: Added `tests/test_core_hotload_inflight.c` with 15 real C integration assertions for concurrent replacement of a module whose service callback is blocked in flight. The fixture's `stop()` unregisters its service through Core; the test checks replacement waits, callback completion, loader and registry uniqueness, stale-route removal and successful replacement. Added `test-inflight` to mandatory `make test`. No production Core or ABI logic changed. Director subsequently confirmed 15/15 PASS and consolidated clean-build/full-suite PASS. This does not cover every shutdown implementation or crash scenario.

## S2 Sentinel collection milestone — 2026-10-27

2026-10-27 — Director-authorized S2 Cyber Security collection milestone: native `modules/s2` first introduced as v1.0.0, retrieving Sentinel JSON from `https://api.stn-labz.com/threats`, tracking source IDs and content signatures, atomically persisting case register, and serving `/s2`. Director reported 10/10 module qualification, 1/1 negative, persistence integration PASS, successful installation, and first live collection of 756 source records (563 explicit MEDIUM/HIGH/CRITICAL). Historical bulk import is not newly occurring attacks. v1.0.1 changed the collection cadence from 900 seconds to **14,400 seconds (four hours)**; operator logs showed 756 unchanged records on the next snapshot. v1.0.2 added automatic baseline IRC synopsis and change-gated updates using Core-mediated `digit.transport.channel.send`; operator confirmed qualification 10/10, negative 1/1, `AUTO_ACTIVE` and `HOTLOAD_ACTIVE`. Automatic unsolicited IRC delivery itself is **not yet independently confirmed** by an IRC receipt. v1.0.3 replaced `/s2` Unix epoch reporting with a readable UTC collection timestamp; operator supplied the working response `2026-10-27 19:10:30 UTC`. Full source evidence/event history, independent correlation, investigative findings and Ollama-assisted narratives remain unimplemented and are not represented as restored. No Core or shared ABI changes for S2.

## Qualification and operational evidence snapshot — 2026-10-27

- **Core:** Director reported clean source build and full configured `make test` PASS; revised Core installed and operational. Six suites individually passed: 20 static source checks; 20 executable Root/policy; 20 executable hotload; 23 threaded routing; 13 self-unregister; and 15 in-flight hotload = **111/111 total checks**. Static checks are not counted as behavioral tests.
- **IRC / Ollama:** IRC 1.1.0 operator qualification 20/20 (negative 1/1) and Ollama 1.0.4 qualification 10/10 (negative 1/1), with activation observed.
- **S2:** Version 1.0.3 status successfully retrieved through IRC: 756 Sentinel source records, 0 new, 0 revised, 756 unchanged, 563 MEDIUM/HIGH/CRITICAL and 0 missing severity; last successful collection **2026-10-27 19:10:30 UTC**. The four-hour collection schedule is active. This proves collection and on-demand report delivery, not incident outcomes or automatic push-to-channel delivery.
- **Further S2 mission work:** evidence-rich investigation histories, independent source correlation, severity-change analysis, corroboration, Ollama-assisted investigative summaries and verification of autonomous IRC delivery. No Publisher or Slack dependency is required for Sentinel ingestion.
- **Governance:** operator-provided technical receipts do not silently grant future development, production changes, incident confirmation, or blanket milestone acceptance. Existing Roadmap approval block is preserved verbatim below.

## Approval
- **Name**: P.Mei
- **Section**: Operations
- **Position**: Founder/CEO
- **Company**: STN-Labz, LLC.
- **Project**: Digit
- **Date/Time**: 20261027:1830Z
- **Status**: Approved
