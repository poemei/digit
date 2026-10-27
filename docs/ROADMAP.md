# Digit — ROADMAP and Development Milestones

**Baseline:** 2026-10-27 repository `main`. This is a continuity plan, not automatic authorization to execute milestones. Digit's established Core structure and the shared STN-LABZ ABI 1.5 are the starting point. Core modifications require direct consent from the Director of the Digit Project.

## Architectural contract

- **Core:** routing and management. A dumb router resolves a registered module destination and dispatches; it does not interpret requests, implement model logic, or format reports.
- **Module controller:** owns routing *inside* its module.
- **Module model:** owns its functional work and data interactions.
- **Module view:** optional, used only when special output/report rendering is needed.
- **Transport (IRC, Slack, Ollama):** its protocol and communication concerns only; no application-level capability selection or intelligence.
- **ABI:** the established Core-mediated registration, invocation, and communication interface. Modules must not directly bind to each other.
- **Root-cause standard:** competency is demonstrated; root causes are corrected. Evidence, not expedient workarounds, determines the component to change.

## Milestone 0 — Continuity baseline

**State: Documentation established; operator review pending.**

- Maintain `docs/CHANGELOG.md` (what actually changed) and this `docs/ROADMAP.md` (gates, remaining work, and decisions).
- Keep module version, Core revision, source commit, build evidence, qualification, installation, runtime results, and operator acceptance distinct.
- Identify any historical documentation links with absent backing files; do not substitute invented qualification receipts.
- **Exit gate:** Director reviews and accepts the continuity baseline.

## Milestone 1 — Core Router and module controller handoff

**State: Initial source committed; three targeted operator live tests PASSED on 2026-10-27. Broader qualification and final operator acceptance pending.**

- Operational evidence supplied on 2026-10-27: IRC 1.0.11 activated, while last observed Ollama activation was 1.0.3; zero replies to nine test messages. IRC source identity subsequently corrected to doctrine-compliant 1.1.0. Ollama source is 1.0.4, but its deployment has not been established.
- **Subsequent operator receipts:** IRC 1.1.0 AUTO_ACTIVE at 16:32:15 UTC; Ollama 1.0.4 AUTO_ACTIVE at 16:43:14 UTC. `digicat: Hello` and `digicat: /chat Hello` both returned replies. `digicat: /unknown test` produced ABI `NOT_FOUND` (result=5) in IRC logs. These supersede the earlier unverified-deployment state without erasing the failure history.
- **Next gate:** establish full Core/IRC/Ollama build and qualification receipts, source-to-installed revision mapping, concurrent dispatch and unregister/hotload behavior, identity and authority handling, bounded arguments/replies, and error delivery. Director's milestone acceptance is separate from observed test passes.

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
- **Critical acceptance gate:** full latest Core qualification (minimum 20/20), module regression tests (minimum 10/10 each), candidate activation-failure injection with incumbent restoration, no stale command handlers, correct loader ownership, and zero-warning target build. Source changes are unverified and not deployed.
- Director controls activation of the revised Core.

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

**State: Future; each module requires its own authorization.**

- Examine Research, S2, and Publish against their actual interfaces and requirements; do not assume unimplemented routes exist.
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
