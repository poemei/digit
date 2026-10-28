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

**State: Core clean build and full configured test suite PASSED on 2026-10-27; revised Core installed and reported operational by Director. Separate Core routing and authority acceptance remains open.**

- **Core qualification:** Director reported `make clean && make && make test` PASS and confirmed installed/operational Core. Individually passed suites: 20/20 static source contracts (not behavioral), 20/20 executable Root/policy behavior, 20/20 hotload integration, 23/23 concurrency, 13/13 self-unregister, and 15/15 in-flight hotload replacement; total 111/111 mixed-type checks.
- **Remaining Core gate:** identity and authority handling, routing error delivery, boundedness and separate Director acceptance. Module activation details belong in `docs/MODULE_ROADMAP.md`.

- Existing ABI 1.5 `register_command`, `unregister_command`, and `invoke_service` are used for controller registration and Core dispatch.
- New Core-owned ingress contract: `digit.router.dispatch`.
- Core selects a registered destination; the module controller selects its own operation.
- **Open inspection:** route naming/collisions; exact command parser behavior; source/authority integrity; response and argument bounds; safe unload and registration withdrawal; error semantics; no hardcoded per-module Core logic.
- **Exit gate:** review full Core source and external ABI contract; compile and run Core positive/negative tests; verify Core routing and withdrawal independently; obtain operator acceptance.

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

## Core qualification and operational evidence snapshot — 2026-10-27

- **Core:** Director reported clean source build and full configured `make test` PASS; revised Core installed and operational. Six suites individually passed: 20 static source checks; 20 executable Root/policy; 20 executable hotload; 23 threaded routing; 13 self-unregister; and 15 in-flight hotload = **111/111 mixed-type checks**. Static checks are not counted as behavioral tests.
- **Module releases and evidence:** maintained separately in `docs/MODULE_ROADMAP.md`; module-level qualification and operation are not Core milestones.
- **Governance:** operator-provided technical receipts do not silently grant future Core modifications, production changes or blanket milestone acceptance. Existing Roadmap approval block is preserved verbatim below.

## Protected Core Root read-only service — 2026-10-28

- Director specifically authorized exposing the actual General Orders to the Identity module through the existing Core-mediated ABI. Core now answers `digit.root.orders.read` requests by reading and validating the immutable Root descriptor and returning the requested order; modules cannot register over that reserved service or modify the Root through it.
- The Core router continues to perform registered dispatch only. Natural-language identity interpretation stays in modules; no IRC or shared ABI modification was required.
- Core source and build-test linking updates committed; the **revised Core requires a new clean build, all qualification tests, controlled installation and operator acceptance**. Existing 2026-10-27 Core receipts cannot be reused to qualify this new revision. Module-specific work is recorded in `docs/MODULE_ROADMAP.md`.

## Approval
- **Name**: P.Mei
- **Section**: Operations
- **Position**: Founder/CEO
- **Company**: STN-Labz, LLC.
- **Project**: Digit
- **Date/Time**: 20261027:1830Z
- **Status**: Approved
