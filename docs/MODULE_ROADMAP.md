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

## Live question routing — Ollama v1.0.7

**State: source committed; operator build/qualification/hotload/integration verification pending.** `chat` now dispatches time-sensitive factual requests to the existing Core-mediated `digit.research.search.v1` service, refuses unsupported offline generation on missing/failed evidence, and returns attributed live search leads. Existing Core and IRC remain untouched. A primary-source schedule verifier is still required before Digit can confidently assert the next launch date/time.

## Silent research chat correction — 2026-10-30

**Source corrected, not yet deployed or qualified.** Core's route reply capacity is 512 bytes; previous research result messages could exceed it and vanish before IRC delivery. Ollama v1.0.8 and Research v1.1.1 constrain replies to 380-byte arrays. No Core/IRC modification. Requires operator build, controlled install and a live chat response test.

## Research v1.1.2 — launch question false-positive fix

**Source committed; live qualification pending.** A real operator report exposed a complete semantic failure: a query for SpaceX's next launch produced a fashion-retailer result. Native Research now strips stopwords and enforces subject terms in results before treating the RSS item as an evidence lead. The offending retailer is part of the regression fixture. Compilation, search quality, installed activation and final schedule verification remain separate operator gates.

## Research 1.2.0 / Ollama 1.0.9 — direct answers, 2026-10-30

**State: source committed; operator tests and acceptance outstanding.** The Director required end-user responses to contain requested facts only, with no links, snippets, or research commentary. The native Research module now queries a free structured launch schedule for SpaceX launch questions, and the conversation controller presents a concise answer. No protected Core, IRC or ABI changes. General live factual questions still lack broad structured verification and should not be represented as solved. Compile, automated tests, admission, activation, live source validation and operator acceptance remain independent gates.

## Director-authorized module-owned research routing — 2026-10-30

**Status: source implemented, not qualified or operator accepted.** The user-directed architecture eliminates the erroneous Research classification and service invocation inside Ollama. New Ask v1.0.0 owns the transport-neutral `chat` command and delegates through Core services; Research v1.3.0 alone claims and answers live research questions, and Ollama v1.1.0 is a pure language-generation provider. Identity and peer-message behavior are retained at the conversation-controller boundary. No Core, router, IRC or ABI source was modified.

**Ordered operator transition:** Build/test all three, verify current active module versions, activate Research 1.3.0 first, then upgrade Ollama 1.1.0 to relinquish the old chat route, then activate Ask 1.0.0 to claim that route. An uncoordinated gap or simultaneous old/new chat ownership may prevent responses or activation. Confirm ordinary chat, Identity, peer-directed messages, a live SpaceX query and a denied/research-unavailable case. No claim of production success until operator end-to-end acceptance.

## General-purpose Research 1.4.0 — 2026-10-31

**Director-authorized scope:** Research must derive terms from the question itself and search across arbitrary subjects, including schedules, software languages and technology documentation, with no per-topic application restrictions. Research 1.4.0 adds a dedicated native query/result/page reading engine under the existing Research controller. A successful structured launch provider remains optional and has a general search fallback; ordinary chat routing and Core remain unchanged.

**Source state:** Subject-independent term extraction, RSS search with bounded retries, source relevance, selective page text extraction and concise evidence-backed answer selection are implemented. The engine is independent of Ollama and needs no three-module response chain. The evidence selection is deterministic and its limits are explicit: it cannot guarantee source truth, freshness, comprehensive synthesis or knowledge retention; there is no declaration of ChatGPT-equivalent research capability.

**Outstanding gates:** local clean compilation; Research positive/negative tests; active-version check under operator control; live searches for a Patriots fixture, PHP/C/Go documentation and unrelated results; operator end-to-end acceptance. No new work or runtime activation is implied by source commits.

## Research 1.4.0 — Multiword subject relevance correction (2026-10-31)

**Unreleased, source correction only; operator acceptance pending.** Corrected the Research engine's result filter: it previously rejected a result whenever the first extracted search word was absent, even when other substantive subject terms matched. This caused a valid abbreviated result such as "Patriots vs Dolphins" to be discarded for a question containing "New England Patriots." The correction requires multiple matching subject words for multiword queries, without requiring the first word. Added a regression fixture covering that case and continued rejection of unrelated SpaceX results. No Core, ABI, Ask, DSD, or transport change. Source inspection and GitHub readback are not compilation, qualification, live-network validation, or operator acceptance; those checks remain outstanding.

## Research 1.4.0 — Qualification follow-up (2026-10-31)

**Unreleased; verification pending.** Operator ran `make -C modules/research clean all test`: compilation completed; qualification failed 72/74 at test lines 116 and 118. The new abbreviated-Patriots fixture was rejected because the previous relevance change still required two matches, while the fixture contained only `patriots`. Line 118 was a dependent assertion against the empty result. Updated Research to admit one distinctive query term only when it is present in both the result title and excerpt, retaining the multiword threshold otherwise. Added a negative fixture for a generic overlapping schedule term. No Core, ABI, Ask, DSD or transport changes. This source correction has not yet been compiled, tested or operator-accepted.

## Research 1.4.0 — Duplicate relevance-term correction (2026-10-31)

**Unreleased; operator retest pending.** The operator compiled Research, but qualification returned **74/75** with a failure at line 122: an unrelated sports result passed the rocket-launch schedule relevance filter. Root cause: the search-term expansion appended `schedule` to a query already containing `schedule`, while the result scorer counted each duplicate as a separate match. Deduplicated query terms during scoring and added a negative fixture with deliberately repeated words. The existing positive abbreviated-subject fixture remains. This source change has not been compiled or qualified following correction; no installed runtime claim. No Core, ABI, Ask, DSD or transport change.

## Research 1.4.0 — Live acquisition failure diagnostics (2026-10-31)

**Unreleased; source committed, runtime validation pending.** Operator IRC and DM reproductions continue returning `I couldn't verify that information right now.`; DSD reports a successful invocation of `digit.research.answer.v1` but does not expose its internal evidence status. The external acquisition engine previously returned the same failure for transport/TLS/HTTP errors, non-RSS/empty or rejected results, and answer selection without temporal evidence. Added redacted stage-specific stderr diagnostics for HTTP status/libcurl result, RSS presence, accepted-hit count and evidence-selection outcome. No URLs, queries or retrieved page contents are logged. These diagnostics identify where the runtime fails; they do **not** themselves restore retrieval, demonstrate a provider failure, or qualify the module. Root cause of the live external failure remains to be established from active-runtime stage logs. Existing Core, Ask, DSD and shared ABI unchanged. No deployment, installation or acceptance inferred.

## Research v1.4.1 — Hotload admission version correction (2026-10-31)

**Unreleased; operator activation and qualification pending.** Operator runtime evidence at 2026-10-31T16:47:25Z reported `MODULE HOTLOAD_DENIED` for Research with `reason=version_not_incremented_or_regressed`. The source had been revised repeatedly while its compiled descriptor and `module.conf` remained at 1.4.0, preventing the active module version guard from admitting the changed artifact. Advanced both version identities to **1.4.1** under approved `docs/20260906.0_VERSION_NUMBERING.md` revision arithmetic, and updated Research README accordingly. No Core/ABI changes. The version fix addresses hotload admission only; research live-acquisition failure, new qualification results, installed activation and operator acceptance remain unverified.

## Research v1.4.2 — Operational logging correction (2026-10-31)

**Unreleased; verification pending.** Operator confirmed Research diagnostics absent from `/opt/digit/logs/digit.log`. Research's audit and engine stage diagnostics wrote to stderr rather than Digit's established log. The Module ABI 1.5 provides no Core logging callback and the executable does not advertise an exported module logging contract. Research now appends bounded, redacted audit and acquisition stage entries to the existing `STNLABZ_LOG_PATH` using a module-owned helper, without creating another log or changing Core. Descriptor and manifest advanced from 1.4.1 to **1.4.2**, with README aligned. This fixes the diagnostic destination, not a demonstrated external search failure. Native compilation, tests, log permissions, activation and operator acceptance are unverified.

## Research 1.4.3 — General search acquisition replacement (2026-10-31)

**Unreleased; source implemented, qualification and live verification outstanding.** Operator-provided Bing RSS response to `new england patriots playing` contained unrelated general-news homepages. Journal evidence showed successful RSS retrieval but zero accepted subject results. The acquisition source—not the relevance threshold—was preventing researched answers. Replaced the general-purpose engine's Bing RSS acquisition with keyless DuckDuckGo Lite HTTPS HTML result acquisition, including bounded title/snippet extraction, subject filtering, and safe HTTPS destination validation; original RSS parser remains only for deterministic regression compatibility. Added generic sports and programming search fixtures and an unsafe URL rejection fixture. Research descriptor, module.conf and README advance together to **1.4.3**. External HTML can change or be blocked; source changes are not evidence of live success. Legacy `digit.research.search.v1` remains a separate pre-existing Bing RSS path; it is not the Ask-owned general answer service. No Core, ABI, Ask or DSD changes. Await native clean build/tests, operator installation/hotload, provider response validation and live end-to-end acceptance.

## Research 1.4.4 — DuckDuckGo result URL decoding (2026-10-31)

**Unreleased; native qualification and live acceptance pending.** Investigation of v1.4.3 found that the HTML parser only decoded `uddg` result URLs if the raw href contained `duckduckgo.com/`; DuckDuckGo Lite can emit root-relative `/l/?uddg=...` redirect links, causing otherwise relevant results to be rejected before subject scoring. It also decoded a redirect's trailing `rut` tracking parameter into the destination URL rather than stopping at the query delimiter. Research now decodes recognized DDG redirect links locally, stops at the end of `uddg`, rejects non-HTTPS/private destination links through the existing URL guard, and retains encoded direct destination URLs. A snippet scan pointer bound was corrected to remain within the input buffer. Added positive fixtures for root-relative and scheme-relative DDG links and a negative fixture for an unsafe target. The compiled descriptor, manifest and README advance together to **1.4.4**. Per the operator's module convention, the Research descriptor is placed near the top of the primary C source with forward declarations for lifecycle functions. Tests also cover HTML-encoded direct result URLs. Provider availability, tests on the operator host, live query success and operator acceptance are not yet established. No Core, ABI, Ask, DSD, or other module was changed.

## Research v1.4.5 and Ask v1.0.1 — Addressed queries and failure transparency (2026-10-31)

**Unreleased; native tests and live verification outstanding.** Operator IRC/DSD records show a generic failure reply and successful Research service invocation; code inspection confirmed Ask discarded specific unavailable evidence. General search-term extraction included `digicat,` as a term and failed to recognize `when are` for temporal search refinement. Research now removes a bounded leading addressee before interrogative questions, expands temporal intent handling, and distinguishes search transport failure, zero relevant results, and insufficient factual detail. Ask relays Research's unavailable explanation without changing service contracts. Regression fixtures cover the original addressed question and unavailable reply. Research descriptor and manifest advance to 1.4.5; Ask descriptor and README to 1.0.1. Earlier journal records reflect the Bing implementation and do not verify the current DuckDuckGo path. Native compilation, tests, installation, hotload and operator acceptance not performed. No Core, ABI or DSD changes.

## Research v1.4.6 — Exact search construction diagnostics (2026-10-31)

**Unreleased; operator testing pending.** At the operator's request, general search now logs each constructed query and full URL in the existing `/opt/digit/logs/digit.log` before HTTPS retrieval using `RESEARCH SEARCH_QUERY terms=...` and `RESEARCH SEARCH_REQUEST url=...`. Search URL build failures are separately logged. Increased the module-owned diagnostic buffer to fit bounded, fully escaped constructed request URLs without truncation or silent suppression. Search URLs contain the user-derived query, so log permissions should remain restricted. The Research descriptor, module.conf and README advance consistently from 1.4.5 to **1.4.6**. This is a diagnostic addition only; it does not demonstrate correct provider responses or live researched answers. Native compilation, tests, hotload and runtime log verification not performed. No Core, ABI, Ask or DSD changes.

## Research v1.4.7 — Preserve retrieved evidence instead of silencing source leads (2026-10-31)

**Unreleased; native build, tests, hotload and operator acceptance pending.** Operator runtime evidence at 17:38:19 UTC established six accepted DuckDuckGo results and zero qualified facts for the requested Patriots schedule. Source inspection identified a source-selection hard cutoff that rejected excerpts shorter than 35 characters or lacking a recognized date and discarded all other source leads. The Research selection path now returns source-attributed excerpts when sufficient, or a clearly unconfirmed source lead (including source URL when the transport can fit it), rather than returning an empty result for six accepted hits. It records per-hit title, excerpt, URL, relevance and recognized date in the existing restricted Digit log for operator inspection. ISO-form dates are recognized. Schedule query refinement no longer duplicates `schedule`. Page text extraction capacity increases, while the code no longer blindly overwrites a relevant excerpt with the first 400 characters of page boilerplate. Tests include source-lead and ISO-date fixtures. This does not assert the Patriots date was established or that the module was built or accepted. Module descriptor/manifest/README advance to 1.4.7; no changes to Core, shared ABI, Ask, or DSD.

## Research 1.4.8 — Repair truncated search excerpts and bounded source fetch (2026-10-31)

**Unreleased; operator validation pending.** Runtime receipt at 17:48 UTC showed six accepted DuckDuckGo hits but excerpts truncated to `THE`, `Full`, and similarly short fragments. The live source parser stopped at the first inner HTML element; the fix extracts content through its corresponding closing element, preserving nested bold/highlight text. The operator's log also showed curl code 23 with 508403 bytes fetched; the existing 512 KiB cap was reached. The bounded source-page limit increases to 4 MiB, text extraction to 96 KiB, and HTTP logs include the cap. A regression fixture covers nested bold/strong content and date extraction through Research's answer selection. Descriptor/manifest advance to 1.4.8. No Core, ABI, Ask or DSD changes. Native compilation, regression execution, hotload and live question success remain unverified; a larger limit does not guarantee a 4+ MiB source can be read.

## Research v1.4.9 — Repair failed qualification fixture and ignored write result (2026-10-31)

**Unreleased; source update only.** Operator supplied actual `make -C modules/research clean all test` receipt: shared object and test binary compiled; 112/113 assertions passed, with `FAIL line 205`. Inspection identified the new nested-snippet regression passing a shadowing `const char *html` as the writable answer argument; `sizeof html` represented only pointer width and failed the answer function's documented minimum capacity. Corrected that regression in the **complete** `tests/test_research.c` using a mutable 448-byte destination. The engine's `research_runtime_log` now checks `write` results and retries interruptions/handles partial writes, addressing the operator's compiler warning. Research descriptor/manifest advance to 1.4.9. Existing Core, ABI, Ask and DSD unchanged. **Post-correction native build/test, install and live verification have not been performed.**

## Research 1.4.10 — Source evidence extraction and precision (2026-10-31)

**Unreleased; post-change tests and live acceptance pending.** Operator demonstrated six relevant search hits but only generic schedule-directory summaries, followed by an answer that merely repeated a source link. Inspection established the Research engine fetched up to two source documents and logged date presence without using page text for answer selection; the first six results also blocked later, higher-information leads. The general page reader now selects dated source passages (including a source-format-conditional WEEK schedule row), passes them into the existing answer selection, and logs excerpt provenance. Time-sensitive refinements continue when the first results lack dated evidence; the six-slot hit buffer permits higher-evidence replacements. Source text cap is raised to 512 KiB. No team-specific or company-specific allowlist, new dependencies, or Core/Ask/ABI changes. Research descriptor and manifest advance to 1.4.10. Regression fixture added for source-page event date and generic dated context; full native compilation, tests, installation, and live result not performed.

### Research 1.4.10 test coverage extension (2026-10-31)

The page-evidence regression now exercises HTML-to-text conversion followed by dated passage selection, and schedule-row extraction handles December-to-January date rollover. Source changes have been committed; native module compilation, full test execution, installation, and live operator acceptance remain unverified.

### Research 1.4.10 source coverage and selection boundary

The operator's accepted results included an official schedule at index 2, while the previous reader examined only indexes 0 and 1. The source reader now examines up to four undated hits until the bounded page-read budget is exhausted, with per-index read outcomes, so eligible official pages beyond the first two are not ignored by position alone. Schedule-formatted event rows are considered only for a schedule-like question. Source changes committed; post-change native tests and live qualification are not yet available.

## Research v1.5.0 — Exact operator-directed search and event qualification (2026-10-31)

**Unreleased; native tests and live acceptance not performed.** Operator provided 18:05:23Z evidence: Research appended `official` to the requested search, accepted 6 results and reported an unrelated navigation string (including many historical years) as a dated game answer. Confirmed source defects: automated query suffixes and generic date-word matching treated page navigation as evidence of a future scheduled event. The general Research request now uses the exact operator question with only the addressed assistant name removed; no invented search refinements are sent. For future schedule questions, an actual future date with a nearby event time is required and general directory source leads do not produce false positive answers. The source-level correction is subject-neutral; no team-specific facts were inserted. Updated qualification tests cover exact outgoing question and false schedule answers. Descriptor/manifest advance from 1.4.10 through the authorized revision rollover to 1.5.0. No Core, ABI, Ask or DSD modifications. GitHub commits are source status only: no native compilation, tests, installation or operational evidence has been supplied for v1.5.0.

### Research 1.5.0 source-level corrections and diagnostic scope

The Research term builder no longer appends search modifiers. The outbound HTTPS request sends the user's exact wording after the leading conversational addressee. For next-event questions, the answer selector requires a future dated event with a nearby time; date-like navigation fragments cannot be returned as successful evidence. Calendar month recognition excludes player names that begin with a month abbreviation (for example `Maye`). Regression fixtures now include the verbatim Patriots question and both navigation-year and non-month-name false positives. Complete native compilation, executable regressions, live source interpretation and operator acceptance remain outstanding.

## Research 1.5.2 — Fair result formatting (2026-10-31)

Source inspection identified that the existing short reply formatter spent the entire remaining reply budget on the first verbose result. The correction uses per-result budgets, preserving full URLs and source order so later results are not suppressed by oversized snippets. This is limited to Research; the established ABI, Core and Ask remain unchanged. New regression checks long excerpts in a 350-byte buffer. Source commit is not a native qualification receipt: compilation, tests, installation and live routing remain unverified.

## Research v1.5.3 / Ask v1.0.2 — Source evidence to language inference (2026-10-31)

**Source committed; native build, qualification, installation, activation and operator acceptance NOT verified.** Operator reproduced Research returning generic schedule links and page fragments rather than the requested Patriots game information. Inspection established that Ask previously returned `RESEARCH_EVIDENCE` directly to the conversation without invoking `digit.llm.generate`; its unit tests explicitly required zero language-generation calls for retrieved results. Research's conversational evidence formatter additionally limited results to 440 bytes.

Ask now forwards bounded research evidence and the operator's question through the existing replaceable language-generation service, explicitly marking source excerpts as unverified data and instructing the provider not to invent missing facts or emit link listings. Research increases its existing Ask-facing response budget to 1700 bytes without altering DuckDuckGo acquisition, Core, ABI, or transport. A safety-denied request continues to be denied. Research unavailable/service failure can receive an inference-assisted explanation, not a fabricated current fact. Ask regression assertions have been updated to check that source evidence and the original question reach the language service.

**Limitations:** A search result without the actual date/time cannot establish a factual next-game answer. Source-page reading, retrieval relevance, the hardcoded Research conversational-claim gate, and LLM factual grounding remain unverified for end-to-end correctness. No operator-host compilation or live DuckDuckGo/Ollama execution has been performed. Do not call this a verified repair until the operator demonstrates results with the installed modules.

**Operator checks required:** `make -C modules/research clean all test` and `make -C modules/ask clean all test`, verify module versions and controlled activation, then repeat the ordinary console Patriots question and unrelated research and safety-negative requests. Do not advance development without the operator's explicit acceptance.

### Research 1.5.3 qualification assertion correction — 2026-10-31

Operator executed `make -C modules/research clean all test` after the 1.5.3 source update. Shared module and test binary compiled. Regression reported **204/205 PASS**, failing line 148 because `tests/test_research.c` still asserted descriptor patch version 2 even though the module descriptor was advanced to patch version 3. Corrected the stale assertion to expect 1.5.3 in the complete test file. No engine, Core, ABI, or runtime logic changed in this correction. **Post-correction build/test, installation, activation, and operator end-to-end acceptance remain unverified.**

## Research 1.5.4 — Remove unauthorized keyword admission gate (2026-10-31)

**Source updated, qualification and operator acceptance pending.** The Director explicitly required unrestricted Research subject coverage for safe requests. The existing `research_claims()` hardcoded phrase list previously bypassed external search for legitimate conversational requests that lacked a listed phrase, returning `RESEARCH_NOT_APPLICABLE`. Removed that phrase-matching gate, preserving `forbidden()` and the established rate limit and HTTPS guards. The conversational Research service now attempts acquisition for every nonempty permitted request delivered by Ask; Identity and peer-directed routing remain Ask responsibilities. No new subject-specific code, dependencies, Core/ABI modifications, or production answer claims. Whole-file regression updates test safe novel wording and verified safety-denial invocation behavior. **Trade-off:** greeting and non-research conversational text now also cause Research requests, increasing search utilization; the existing budget may return unavailable. Only operator-controlled build, tests, installation and live end-to-end verification can establish acceptance.

## Ask 1.0.3 / Ollama 1.1.1 — IRC agent-loop containment (2026-10-31)

**Source committed; native compilation, regression tests, installed activation and live loop safety remain unverified.** Operator reported that the unrelated query `when are the New England Patriots playing again?` produced `rictus: when are the New England Patriots playing again?`, creating a repeated exchange with another IRC participant. Source inspection established the Ollama global prompt contained explicit `rictus:`-style examples applied to every inference request while Ask already owned explicit peer routing. This is a direct conflicting source influence, though the precise live inference prompt/response trace was not captured. Ollama's global instruction no longer names any other participant or instructs ordinary inference to initiate IRC dialogue. Ask now strips generated leading IRC nick prefixes from ordinary answers before using the regular reply path, while preserving explicit `ask/tell/say` target routing. Regression tests cover an unsolicited nickname prefix, confirm no peer-send invocation, preserve explicit relay behavior, and verify the previous named example is absent from global Ollama request construction. No Core, router, IRC transport or shared ABI changes were made; neither server reboot nor source commit proves successful repair. The factual-search-answer issue is separate and remains unresolved.

## Research 1.5.5 / Ask 1.0.4 — Source evidence handoff (2026-10-31)

Operator symptom: an ordinary future-event question returned descriptions of schedule links, without the requested facts. The live DuckDuckGo response was not captured, so its actual contents remain unknown.

Root causes visible in source: the Research formatter prioritized URLs rather than snippets; Ask arbitrarily cut Research's output to 1300 bytes; and its prompt emphasized absent facts over answering. The rebuilt full source files preserve the single exact DuckDuckGo query, service ABI, security and rate limits, and topic-neutral behavior. Search results now convey title, excerpt, and full URL together. Ask forwards the original question, retrieval time, and as much formatted evidence as its existing inference contract permits. A synthetic regression checks that an event time survives formatting. No page crawling, external provider, Core modification, or deployment is involved.

Source committed; compilation, regression execution, activation, and live operator acceptance remain UNVERIFIED. A single search cannot guarantee the answer if its snippets omit the requested fact. Required checks: make -C modules/research clean all test; make -C modules/ask clean all test; operator-selected end-to-end client test. AWAITING OPERATOR END-TO-END TESTING AND ACCEPTANCE.

## Research 1.5.6 / Ask 1.0.5 — KISS search-to-IRC restoration (2026-10-31)

**Authorized scope:** user question string → a single DuckDuckGo search → retrieved results → human-readable formatting → IRC. No inference, webpage crawling, secondary search or subject-specific answers.

**Root cause demonstrated in source:** Ask 1.0.4 routed every ordinary Research result to digit.llm.generate, converting source listings into unverifiable generated claims. Research 1.5.5 also emitted up to 1699 bytes for Ask while the routed IRC reply boundary is 447 bytes, causing downstream truncation. The operator observed an incorrect generated game date and venue; the /research command showed only a brief unrelated advertising result and an official schedule-page listing, not the claimed game details.

**Corrected source:** Ask 1.0.5 now sends Research's formatted results directly through its ordinary reply callback. The exact original user question is passed unchanged to Research, and there is no inference call for ordinary search. Research 1.5.6 bounds the Ask-facing text at 439 characters to keep complete result URLs within IRC's established boundary. The existing single-query search engine, input safety gate, rate limit, parser, service ABI, explicit /research command, authoritative Identity handling and explicit peer messages are preserved.

**Qualification evidence:** source commits and test-source assertions only. Regression tests now require exact-query transmission, verbatim formatted results, no LLM calls for normal questions, and unchanged denial/unavailable/identity/peer handling. Native builds, executed test results, activation, deployment, and live IRC acceptance have not been performed here. Operator qualification: make -C modules/research clean all test && make -C modules/ask clean all test. Do not claim that a result-snippet listing contains facts that its text omits.


## Research 1.5.7 — Generic upcoming-event search selection (2026-10-31)

Operator requirement: interpret "when ... again" as a future-time constraint, search for the relevant upcoming schedule using one query, and return the nearest supported result with event details. No Patriots/NFL-specific code, model inference, extra providers, page crawling, or Core changes.

Root-level implementation: in the Research engine, explicitly future-directed WHEN questions are rewritten into a subject-based query with today's UTC date and generic schedule/date/time/location terms; otherwise the original query is used. A known DuckDuckGo advertising disclosure link is excluded from parsed organic results. In the Research reply formatter, a future-directed question selects the closest future date found explicitly in retrieved titles/excerpts, retaining that result's snippet and URL; all other searches still deliver normal result listings. A result lacking date/time/venue/broadcast evidence cannot provide those facts, and this module must not invent them.

Regression source adds dynamic UTC-relative dates, out-of-order upcoming search results, ad exclusion, unchanged query handling for unrelated topics and single-request assertions. Source changes committed. Native compilation, executed tests, live DuckDuckGo coverage, installation and operator acceptance NOT VERIFIED. No automatic installation.


## Research 1.0.0 — Shared ABI 1.5 integration correction (2026-10-31)

Operator reported an initial Research build failure caused by an unauthorized libxml2 dependency. The dependency was removed and replaced with an internal HTML text extractor; existing libcurl remains the only networking library. The second operator build compiled `research.so` successfully but rejected the qualification test due to an undeclared descriptor prototype; that declaration was corrected.

The shared `stnlabz/ABI` repository's README confirms the ABI 1.5 descriptor export and stop/unregister lifecycle. The Research ABI source contract uses the established `stnlabz_abi/module.h`, registers the existing answer/search services through Core, and exports `stnlabz_module_get_descriptor`. The native qualification test now exercises descriptor identity and API compatibility, positive lifecycle registration and stop, invalid-host rejection, and registration-failure rollback. A standard `module.conf` manifest was added to the module package and install target. No Core, Ask, shared ABI, branch or workflow changes are included.

**Verification status:** Source committed. The operator's successful `research.so` compile predates the latest qualification test and manifest updates. Current native `make -C modules/research clean all test`, installation and end-to-end Research/Ask behavior have **not** been independently confirmed. An offline test pass does not establish correct answers to live arbitrary questions; production acceptance remains the operator's decision.

## S2 investigative continuity milestone — 1.3.3

**Objective:** Follow individual Sentinel source records across collection cycles, identify revisions and severity escalations, and add evidence-bounded investigative leads to the DSA assessment.

**Deliverables:** Persistent, access-restricted investigation journal separate from the existing collection register; first/last S2 observation, re-observation counts, source-record revisions, classification increases, and priority lead summaries. Preserve HTTP:BL lookup, existing Rictus candidate-log matching, six-hour collection and publication, Core/ABI 1.5, and human-controlled response.

**Exit gate:** Native S2 build, qualification and negative tests, persistence verification across cycles, source-data privacy review, controlled activation, verified DSA read-back, and explicit Director acceptance. Validate that repeated retained records are not reported as repeated attacks.

**State:** Source changes committed; native execution, live investigative fidelity and public report acceptance remain unverified. Independent incident confirmation, event-by-event source history prior to the journal, and a human-assigned investigative case disposition are not established by this increment.

## S2 1.3.4 — Evidence-specific public investigation findings

**Objective:** Replace indistinguishable high-severity lead narratives with individual source-backed assessments for the DSA report.

**Deliverables:** Per-lead request-category interpretation, supplied observation time, HTTP outcome, related-indicator counts, continuity context and independent verification priority. Preserve raw-identifier, domain, IP, URL and payload redaction, existing collection journal, Core/ABI boundary and six-hour cadence.

**Exit gate:** `make -C modules/s2 clean all test`, actual fixture output inspection, privacy and classification negative checks, controlled S2 activation, DSA page verification and Director acceptance.

**State:** Source committed; native build, live investigative fidelity and final operator acceptance unverified. No claim of independent access-log corroboration or confirmed compromise.

## GitHub module — Cycle 1: read-only repository intelligence

**Objective:** Allow Digit to return evidence-backed information about a human-approved GitHub repository, as a measurable prerequisite for engineering competence.

**Deliverables:** Native `modules/github` module using existing ABI 1.5, explicit allowlist, read-only GitHub REST retrieval, report service and `/github` command. Inspect repository metadata, root/directory entries, source excerpts, commit references, issues, and pull requests. Deny unapproved repositories and malformed paths before network access.

**Exit gate:** Native build, at least ten qualification checks including negatives, full controller/service lifecycle tests, verification against an operator-approved live public repository, report usefulness review, and Director acceptance.

**State:** Cycle 1 source implemented and committed. Native build, offline tests, runtime activation and live GitHub report quality remain unverified. No private repository authentication, cloning, pulling, checkout, diff, commit, push, GitHub workflow, or Rictus integration is implemented. These are outside the current cycle.

### GitHub Cycle 1 build-directory correction — 2026-11-03

**Observed:** Operator ran `make -C modules/github all clean test`; linking `build/test_github` failed because `clean` removed the previously created build directory. **Correction:** Both full compile recipes now ensure the output directory exists immediately before compilation, independent of an earlier Make directory prerequisite. **Gate:** Operator repeat build and offline qualification remain pending; this source correction does not establish successful execution.

### GitHub Cycle 1 first-install allowlist — 2026-11-03

**Observed:** Offline qualification passed (25/25, negative 1/1, lifecycle 29/29), but the operator's install command failed because `/opt/digit/modules/github/repos.conf` was missing.

**Correction:** First install copies `tests/approved.conf` to the production allowlist with `digit:digit` ownership and 0640 permissions. Existing production allowlists are preserved on reinstall. No read/write capability expansion.

**Exit gate:** Operator repeats installation and confirms the seeded configuration and module activation. Current evidence covers committed source only, not successful post-fix installation.

### GitHub Cycle 1 routing contract correction — 2026-11-03

**Observed:** IRC `/github` attempts reached Digit's Core but reported result 7 (`INVALID_STATE`). The existing Core hot-loader is active without a restart, so a missing restart was not an established cause. Code inspection showed Core accepts exactly one routed reply under 512 characters, whereas GitHub 1.0.0 emitted multiple replies for a normal summary and treated the second rejection as failure.

**Correction:** GitHub 1.0.1 emits one bounded command reply and leaves longer evidence in its named report service; descriptor, manifest, tests and documentation synchronized. No Core modification or restart requirement.

**Exit gate:** Post-fix module build/tests, automatic hot-load acceptance and one actual operator-observed IRC response. Source committed; runtime gates not yet independently verified.

### GitHub Cycle 1 — IRC evidence pagination, 1.0.2 (2026-11-03)

**Observed:** Operator confirmed successful live retrieval and IRC response for `poemei/digit`, but the result was cut midway through the root listing by the Core reply limit.

**Implemented:** Read-only `/github` paged commands using `page=N` (1–40) and single bounded Core replies. Full bounded service response, repository approval checks and read-only GitHub REST behavior are preserved. No Core, ABI, GitHub write access, or Rictus policy changes.

**Qualification gate:** Run `make -C modules/github clean all test`, install the 1.0.2 module under operator control, and verify page 1, page 2, final page and invalid-page responses against a live authorized repository. Record exact module admission and command routing results. This gate is not yet complete.

### GitHub Cycle 1 — human-readable reporting (1.0.3), 2026-11-03

**Operator observation:** Live repository lookups worked but returned raw metadata and seven numbered pages rather than useful explanations.

**Correction:** Standard read-only GitHub commands now answer in concise plain English. The default summary identifies only GitHub-supported description, language and observed project structure, explicitly distinguishing organization from tested code correctness. Detailed numbered pages remain optional. Offline checks cover representative project metadata and root entries.

**Acceptance:** Build and qualify 1.0.3, install through the existing hot-loader with no Core changes, and confirm the ordinary IRC reply is both readable and faithful to source evidence. Still pending execution.

### GitHub Cycle 1 — language-agnostic repository discovery (1.0.4), 2026-11-03

**Purpose:** Preserve one read-only GitHub module across repositories written in C, C++, PHP and other languages, including mixed-language projects. No language-specific GitHub module, hardcoded language rejection or Core change.

**Implemented:** Query GitHub's /languages metadata; report up to four prevalent detected languages in English, flag multiple-language repositories and distinguish unavailable language evidence. Offline fixture checks cover C/C++/PHP/Python and plain-language output. Existing module reporting and authorization remain intact.

**Qualification gate:** Operator runs the existing Makefile qualification and install, then validates live mixed-language and unavailable-language responses. Actual source-code comprehension, build verification and certification remain separate, unproven capabilities.

### GitHub Cycle 1 — role-aware file classification (1.0.5), 2026-11-03

**Operator evidence:** Digit was called a C, Makefile and Python project although the application's native implementation is C and Python appears in tests; ChAoS MVC was classified without separating PHP implementation and front-end assets.

**Change:** Inspect actual recursive Git file paths before attributing language roles. Test folders, build tool locations and supporting assets are reported separately from implementation. Source language classification is evidence-limited, supports mixed-language repositories and does not hardcode project identities or assume GitHub's byte counts imply runtime. An incomplete or unavailable tree prevents a determinate role claim.

**Acceptance gate:** Run module tests and install with operator control, then check the two reported repositories against observed file paths. No live post-change receipts yet. Actual code comprehension and verification remain separate, deferred capabilities.

### GitHub Cycle 1 — real-source inspection (1.0.6), 2026-11-03

**Scope:** After operator-demonstrated language-role classification in 1.0.5, introduce bounded actual-source inspection without Core or ABI changes.

**Implementation:** New `/github owner/repo inspect path` reads a GitHub file's Base64 content through the existing HTTPS read-only client, records Git blob SHA, and optionally requests plain-English interpretation using the already-established `digit.llm.generate` Core service. Inference is explicitly limited to the fetched text excerpt, does not establish test results, and falls back to observable code when the model is unavailable. Authorization, path checks and module lifecycle remain unchanged.

**Qualification:** Offline tests cover permission-independent argument validation, Base64 source decoding, binary fallback and source attribution; target Linux compilation and actual Ollama-assisted source analysis are still unverified. Full-file comprehension, cross-file understanding and execution/testing remain outside this increment.

### On-demand coder model selection — Ollama 1.1.2 + GitHub 1.0.7 (2026-11-03)

**Directive:** Engineering inference is critically important; use `qwen2.5-coder:3b` only when engineering work requires it while preserving fast conversational `llama3.2:1b`.

**Implemented:** Added `digit.llm.engineering.generate` as a separate Core-mediated service using the existing inference request/response shape, and updated GitHub `inspect` to request it explicitly. Coding requests use `keep_alive=0` and bounded generation; normal chat retains its original service. No global model switch or Core change. Coding inference failures surface through evidence-only fallback, never through the lite model. Ollama model must be installed by the operator.

**Qualification gate:** Run `make -C modules/ollama clean all test` and `make -C modules/github clean all test`; install Ollama 1.1.2 before GitHub 1.0.7 through existing operator-controlled hotload. Verify lite chat, engineering inspection, unavailable model behavior, and model memory release under realistic load. No host build or live receipts for this increment yet. Exact single-model memory limits require separate host-level configuration and are not claimed here.

### Ask 1.0.6 — return ordinary conversation to the lite model (2026-11-03)

**Observed defect:** `Hello` returned unrelated web-search content; source traced to Ask 1.0.5's unconditional Research routing for every non-peer message.

**Implemented:** Identity first; explicit `research `/`search ` invocation selects Research; ordinary messages select the existing `digit.llm.generate` lite service; peer commands keep their existing route. The independent Research command and dedicated GitHub engineering inference are untouched. Ordinary generated messages cannot silently become directed peer sends from a leading nickname.

**Qualification gate:** Compile and run `make -C modules/ask clean all test`; operator-controlled install of Ask 1.0.6, then verify conversational `Hello`, explicit `research ` results, authorized Identity responses, peer-directed messages and GitHub engineering inspection still function. Source/test implementation is committed but host build, qualification, activation and IRC acceptance remain unverified.

### Ask 1.0.7 — Restore greetings independently of inference (2026-11-03)

Operator witnessed `Hello` produce `Language inference failed` after Ask 1.0.6, with Research remaining functional. Source correction routes standalone `Hello`, `Hi`, `Hey` to deterministic replies after Identity; mixed input remains on the original request route. Source and tests committed. Qualification, hotload, and live acceptance require operator validation. **The lightweight inference service failure for other conversation remains open pending host logs/model availability evidence.**

### GitHub 1.0.8 — Tree pagination (2026-11-03)

**Evidence:** `stnlabz/chaos-mvc tree` returned four root names. Source `everyday_list()` had `shown<4`; page>1 entered an unrelated detailed-report view.

**Implemented:** GitHub tree commands now use item-aligned pagination against the approved GitHub Contents directory. Show count, current page, and exact follow-up `/github owner/repo tree [path] page=N`. Explicitly report unsupported directories exceeding 40 pages; no silent four-item truncation. Existing source/issue/commit inspection paths untouched.

**Qualification pending:** `make -C modules/github clean all test`, operator-controlled install/hotload, and live root and subdirectory page navigation, with no Core changes. GitHub Contents API limits and mutable results between independently fetched pages remain explicit.

### GitHub 1.0.8 qualification correction (2026-11-03)

Operator native compile completed but module qualification failed. Root cause in test fixture: page 3 is out of range for 64 short entries (two valid pages); its `unavailable` response does not display `64 entries`, contrary to an unconditional test assertion. Corrected fixture and added exhaustive entry-presence tests plus explicit navigation/out-of-range assertions; cleaned reported C compiler warnings. Source committed, server build/qualification and hotload acceptance pending.

### Digit Web Transport 1.0.0 — local IPC baseline (2026-11-03)

Director authorized initial native transport implementation; `/digit-chat` ChAoS MVC module remains separate. `modules/web` provides protected Unix socket plus `SO_PEERCRED` caller UID verification and dispatches through the existing Core-mediated `digit.router.dispatch` service, not IRC. No Core or other module changes.

**Gate:** Run `make -C modules/web clean all test`, provision a protected local runtime directory and trusted PHP process UID, manually install/activate and validate end-to-end. Website authentication and per-user permission checks must exist before public browser access.

**Limitation:** Existing router reply cap is 512 bytes, so unbounded/extended web responses are not implemented. Larger response contracts, streaming, persistence, and concurrency are deferred. No build, native qualification or live operation has been verified.

Web 1.0.0 test scope expanded with socketpair round-trip against a mocked `digit.router.dispatch` callback (success and error), plus intact failure counters. Tests remain unexecuted on the operator host.

### Web transport 1.0.1 — 2026-11-03

Approved design: Digit initiates outbound HTTP requests to a local ChAoS MVC `/chat/transport` endpoint, retrieves pending messages, routes them through the existing `digit.router.dispatch` Core service, and posts the reply. This replaces the previous socket transport. Browser interface is `/chat`.

Offline C build and 16/16 tests passed in an isolated environment. Native host build, installation, hotload and web integration are not verified. The PHP endpoint, browser authorization and queue behavior are separate implementation steps. Core remains unchanged and its existing reply length limit still applies.

### Identity test fixture correction — 2026-11-05

Updated the Identity native integration test expectations to match the existing Director-approved mission and General Order text in `data/identity.conf`. The observed WSL2 `13/13` selftest pass followed by approved mission failure was caused by mismatched test literals; no mission or model changes. Source consistency checked; native rerun pending.
