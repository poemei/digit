# Digit — Module Roadmap and Development Milestones

**Scope:** Digit modules only. This document tracks module-specific source, build, qualification, installation, runtime, investigations, transports, and remaining module work. Core routing, protected Core changes, and Core qualification are tracked in [ROADMAP.md](ROADMAP.md). This roadmap does not grant automatic permission to develop or deploy modules. The Project Director retains all authorization and acceptance authority under the governing Core Roadmap.

**Architecture:** Modules communicate only through Core-mediated ABI services and commands. Module controllers own internal routing; models own functional work; optional views format output; transports handle communications. Separate module milestones and releases must not be represented as Core development milestones.

## Module routing and transport history — 2026-10-27

- Operational evidence supplied on 2026-10-27: IRC 1.0.11 activated, while last observed Ollama activation was 1.0.3; zero replies to nine test messages. IRC source identity subsequently corrected to doctrine-compliant 1.1.0. Ollama source is 1.0.4, but its deployment has not been established.
- **Subsequent operator receipts:** IRC 1.1.0 AUTO_ACTIVE at 16:32:15 UTC; Ollama 1.0.4 AUTO_ACTIVE at 16:43:14 UTC. `digicat: Hello` and `digicat: /chat Hello` both returned replies. `digicat: /unknown test` produced ABI `NOT_FOUND` (result=5) in IRC logs. These supersede the earlier unverified-deployment state without erasing the failure history.
- **Updated evidence:** Director reported `make clean && make && make test` PASS on current Core checkout and confirmed installed/operational status. Earlier individual suites passed: 20/20 static source contracts (not behavioral), 20/20 executable Root/policy, 20/20 hotload integration, 23/23 concurrency, 13/13 self-unregister, 15/15 in-flight replacement; 111/111 combined checks of mixed types. IRC 1.1.0 and Ollama 1.0.4 qualification/activation receipts were also supplied (20/20 and 10/10 respectively, with negative tests). Remaining: installed-artifact provenance as needed; broader identity/authority, bounded route error delivery, and explicit Director acceptance.
- **Current classification:** The above IRC/Ollama version and runtime receipts are module evidence, not new Core revision or Core qualification evidence.

### Installation-integrity gate (2026-10-27)

- The Ollama installed artifact did not match its built artifact and was owned by `root:root` instead of `digit:digit` in operator-provided evidence.
- IRC and Ollama Makefiles now enforce user/group, compare built and installed bytes, and stage/rename binaries within the watched directory. Source committed; IRC 1.1.0 and Ollama 1.0.4 activation confirmed by operator. Post-install byte-hash and owner receipts were not supplied.
- Capture `sha256sum`, `stat`, and module activation receipts; then re-run routed IRC tests. Do not infer an installed revision from GitHub source.

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

## S2 Sentinel collection milestone — 2026-10-27

2026-10-27 — Director-authorized S2 Cyber Security collection milestone: native `modules/s2` first introduced as v1.0.0, retrieving Sentinel JSON from `https://api.stn-labz.com/threats`, tracking source IDs and content signatures, atomically persisting case register, and serving `/s2`. Director reported 10/10 module qualification, 1/1 negative, persistence integration PASS, successful installation, and first live collection of 756 source records (563 explicit MEDIUM/HIGH/CRITICAL). Historical bulk import is not newly occurring attacks. v1.0.1 changed the collection cadence from 900 seconds to **14,400 seconds (four hours)**; operator logs showed 756 unchanged records on the next snapshot. v1.0.2 added automatic baseline IRC synopsis and change-gated updates using Core-mediated `digit.transport.channel.send`; operator confirmed qualification 10/10, negative 1/1, `AUTO_ACTIVE` and `HOTLOAD_ACTIVE`. Automatic unsolicited IRC delivery itself is **not yet independently confirmed** by an IRC receipt. v1.0.3 replaced `/s2` Unix epoch reporting with a readable UTC collection timestamp; operator supplied the working response `2026-10-27 19:10:30 UTC`. Full source evidence/event history, independent correlation, investigative findings and Ollama-assisted narratives remain unimplemented and are not represented as restored. No Core or shared ABI changes for S2.

## Qualification and operational evidence snapshot — 2026-10-27

- **IRC / Ollama:** IRC 1.1.0 operator qualification 20/20 (negative 1/1) and Ollama 1.0.4 qualification 10/10 (negative 1/1), with activation observed.
- **S2:** Version 1.0.3 status successfully retrieved through IRC: 756 Sentinel source records, 0 new, 0 revised, 756 unchanged, 563 MEDIUM/HIGH/CRITICAL and 0 missing severity; last successful collection **2026-10-27 19:10:30 UTC**. The four-hour collection schedule is active. This proves collection and on-demand report delivery, not incident outcomes or automatic push-to-channel delivery.
- **Further S2 mission work:** evidence-rich investigation histories, independent source correlation, severity-change analysis, corroboration, Ollama-assisted investigative summaries and verification of autonomous IRC delivery. No Publisher or Slack dependency is required for Sentinel ingestion.
- **Governance:** operator-provided technical receipts do not silently grant future development, production changes, incident confirmation, or blanket milestone acceptance. The governing approval record remains in `docs/ROADMAP.md`.

## Required module engineering sequence

`Requirement → Inspection of example.zip and Module Creation Guide / AI Module Creation / AGENTS.md → Evidence → Proposal → Director authorization → Full-file implementation → Module build → Positive/negative qualification → Activation → End-to-end operator verification → Separate acceptance`

- At least 10 module qualification tests including a passing negative test, as applicable under existing project rules.
- A successful Core suite is not a substitute for testing a specific module. Likewise, passing a module test does not qualify Core.
- Unfinished S2 investigative functionality remains explicitly open even though ingestion and `/s2` status reporting are operational.

## 2026-10-28 — IRC v1.1.1 ordinary channel traffic log discipline

- Director-authorized: removed routine `PRIVMSG ignored: channel message did not address Digit` logging. Unaddressed channel messages remain silently ignored; routing, security checks, malformed-message diagnostics and failure logging remain unchanged.
- IRC module patch version raised from 1.1.0 to 1.1.1 to allow qualified hotload. Build, tests, installation and live operator acceptance pending.

## 2026-10-28 — Identity & Mission native module v1.0.0

- Director-authorized native `modules/identity` module added with a read-only approved identity/mission record, internal data model, Core-registered `/identity` controller, 10 qualifying model checks including a negative check, and explicit test/install targets.
- The `mission` field preserves the Director's exact approved mission statement. `/identity mission` and `/identity who` return deterministic responses without invoking Ollama.
- **Protected Core boundary:** General Orders remain in Core; no Core or ABI read service currently exposes them to modules. `/identity orders` therefore explicitly declines to invent an answer. No duplicate mutable copy of Core orders was created.
- **Routing boundary:** Unprefixed natural-language inquiries still default to Ollama's `chat` route; `/identity` is explicitly routed and does not yet intercept arbitrary conversational inquiries. A separate Director-approved solution is required.
- Module source committed; host build, qualification, activation and operator client acceptance unverified.

## 2026-10-28 — Director-authorized authoritative identity question routing

- Added protected Core read-only `digit.root.orders.read` service, returning validated Root General Orders through the existing ABI invocation path without exposing mutation.
- Identity module v1.1.0 now retrieves General Orders from Core, registers a separate deterministic identity-answer service, and recognizes mission, who-are-you, and first/second/third (1st/2nd/3rd) question variants.
- Ollama v1.0.5 chat controller delegates these questions via Core-mediated Identity service; unrelated questions continue to normal Ollama generation. No IRC or shared ABI changes.
- Updated Core test linking for Root providers. Source committed; host build, tests, installation, runtime and operator acceptance unverified.

## 2026-10-28 — Identity General Order response repair (v1.1.1)

- Director authorized eliminating the failing new Core lookup mechanism. The added Core order lookup service was removed, while immutable Root identity and existing protected General Orders remain intact.
- Approved conversation answer copies of all three General Orders are now required fields in Identity's standalone `identity.conf` record; module v1.1.1 responds to direct and natural-language order requests without the failing Core query.
- Identity answer service and Ollama conversational delegation remain in place. IRC and shared ABI unchanged. Tests expanded to compare all three approved data records and group question classification.
- Source-only evidence: no compilation, module qualification, installation or live IRC verification claimed.
