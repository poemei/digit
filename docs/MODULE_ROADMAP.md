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

## 2026-10-28 — Identity v1.1.2 response presentation

- Director requested natural first-person responses for General Order questions. Identity now renders each approved order as an `I must` statement, with the original data unchanged.
- Core, IRC, Ollama and ABI unchanged. Source committed; tests and live verification remain pending.


## 2026-10-28 — Ollama v1.0.6 peer-action transfer from Rictus

**State: Source implemented; module qualification, activation, end-to-end verification and acceptance pending.**

Director authorized transferring the current proven Rictus peer-address/action behavior into Digit Ollama while preserving Digit-specific identity handling.

Digit Ollama now:

- keeps Core-mediated Identity answers ahead of model generation
- instructs generated peer speech to use the exact IRC nickname followed by a colon
- preserves statement intent for `say`/`tell`
- preserves question intent for `ask` and does not answer on the peer's behalf
- deterministically restores the peer prefix when generation omits it
- recognizes the currently proven `say ... to <nick>`, `tell <nick> ...`, and `ask <nick> ...` forms
- sends recognized peer actions through the existing Core host `send_message` boundary
- leaves ordinary conversational replies in their original conversation context

Module descriptor advanced from **1.0.5 to 1.0.6**.

Shared ABI, IRC transport implementation, endpoint, model selection, and Digit Identity-service precedence are unchanged.

Required gate: `make -C modules/ollama clean all test`, qualification evidence including the negative test, controlled install/hotload receipt for 1.0.6, live channel peer action, live DM-origin peer action, and explicit Director acceptance.

## 2026-10-29 — S2 native sai-dsa publication source v1.1.0

- Director specifically requested native publishing **inside Digit S2** rather than external Python scripts or a standalone Publisher module.
- Introduced S2 public-report model using only source-backed numeric findings and reviewed prose. The public brief includes source severity and response evidence, investigation gaps, conclusions, and a final paragraph on potential business impact; unverified Rictus case correlations are deliberately excluded.
- Introduced a native libcurl site client for the existing OS-owned Digit credentials, CSRF-protected site login, and Pages update restricted to `sai-dsa`.
- S2 retains its four-hour Sentinel collection, with separate six-hour publishing opportunity and persisted last-published timestamp. Previously published content is not intentionally replaced on invalid source or failed authentication.
- New tests check report format and non-disclosure of representative sensitive input.
- **Known limitation:** Current S2 persists an ID/hash register rather than the full source evidence and does not access Rictus event history; therefore the full per-threat investigative report in the operator's example is not yet produced. No claim of completed cross-source research.
- Source committed only. Native build, selftest, module qualification, site mission-granted login, publication and live interval are **unverified**. No Core, IRC, website Core or shared ABI edits.

## 2026-10-29 — S2 public-reader research and editorial integration v1.2.0

- Director identified the existing host-only plain-text Project Honey Pot HTTP:BL key path `/opt/digit/auth/httpbl.key`; updated S2 to read it with private-file checks, without copying or logging the secret.
- S2 now queries HTTP:BL for up to three eligible public IPv4 source addresses in the latest Sentinel JSON, and differentiates search-engine classification from suspicious, email harvesting and comment spam indicators.
- Human-readable public report text explains website scanning, what source checks were completed, and limits of attribution. Optional Ollama editorial assistance uses the existing `digit.llm.generate` service for a single general-education sentence only, without exposing source observations. S2 controls final fact and business-effect wording; fallback to static prose.
- Maintains native S2 and six-hour SAI-DSA publishing cadence. No Core, IRC, ABI, site Core, Python, or separate publisher edits.
- **Pending:** Compilation, positive/negative qualification, key-file access validation, HTTP:BL resolution behavior, and site publication acceptance. Current S2 source still lacks full Rictus correlation and durable per-case investigative evidence needed to replicate the historical comprehensive report.

## 2026-10-30 — S2 v1.2.1 hourly operator test cadence

- By operator direction, changed the native S2 Sentinel polling interval from 14,400 to 3,600 seconds and the SAI-DSA publishing eligibility interval from 21,600 to 3,600 seconds for testing.
- Updated module descriptor and module.conf to 1.2.1; no Digit Core, IRC, credentials, or website source changes.
- Existing last-published stamp continues to govern eligibility; a collection or publishing failure may prevent an hourly public update. Configuration values document the compiled intervals and are not dynamically loaded by S2.
- Source committed; installed runtime activation and live hourly publications are not verified.

## Director-authorized Live Research module — 2026-10-30

**State: source implemented; compile, qualification and operator client acceptance pending.**

The native `modules/research` v1.0.0 module registers `/research` and Core-mediated `digit.research.search.v1`. It uses a fixed HTTPS search-provider endpoint, strict SafeSearch, read-only requests, bounded responses, guarded credentials, and a per-minute query budget. Source excerpts are untrusted and explicitly not evidence of independently verified events. A local deterministic denial gate covers selected harmful actionable requests but does **not** claim comprehensive content-safety classification.

No protected Core or other module was changed. Natural-language DSD integration remains a separate operator-authorized step. No build/test, deployment, live connection, end-to-end result or acceptance is claimed. Next required check: `make -C modules/research clean all test`, then authorized credential provisioning and operator-controlled activation/client testing.

## Live Research v1.1.0 — Director correction: no paid provider

**Source committed, verification pending.** The paid Brave credential dependency was removed. A fixed, keyless public HTTPS RSS search source supplies unverified headline evidence with a strict bounded parser. The interface is unofficial and cannot guarantee broad research coverage. Protected Core and DSD/transport modules remain untouched. Operator compilation, live-source validation, qualification, installation, and DSD consumer integration remain open gates.
