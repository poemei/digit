# Digit — CHANGELOG

This is the source-change record for Digit. Dates are source dates, not deployment or acceptance dates. Module versions are independent of Digit's agent release. A GitHub commit, successful compilation, or a module reconnect does not establish qualification or operator acceptance.

## 2026-10-27 — Concurrent Core routing withdrawal test coverage

- Added threaded `tests/test_core_concurrency.c` exercising registered service and command handlers while a separate thread unregisters them, including in-flight completion, blocking withdrawal, removal, slot reuse and no post-withdrawal dispatch.
- Added `test-concurrency` to mandatory `make test`, using pthreads and the real `src/module_host.c` implementation.
- These tests do not emulate uncooperative third-party modules, self-unregistration, or a complete concurrent hotload sequence. All new concurrency checks are source committed and **not executed** as of this entry.

## 2026-10-27 — Hotload integration audit assertion correction

- Operator ran the integration harness: 19/20 assertions passed. The only failure was the test expecting `replacement_active` in the operator module log.
- Source inspection confirms `src/logging.c` deliberately omits success-reason text for `HOTLOAD_ACTIVE`; the event name itself is recorded.
- Corrected the complete `tests/test_core_hotload.c` file so its audit check counts exactly two `HOTLOAD_ACTIVE` events (initial admission and replacement), instead of expecting an unlogged reason string. No production Core logic or ABI modified.
- **Re-run pending:** this correction has not yet been executed; 20/20 integration must not be claimed until independently observed.

## 2026-10-27 — Core hotload executable integration gate (source committed; run pending)

- Added four independently compiled native-module fixtures (original, 9/10 unqualified update, qualified update with deliberately failing start, qualified successful update).
- Added 20 runtime integration assertions in `tests/test_core_hotload.c` covering discovery, admission, rejection without disturbance, rollback to previous ACTIVE module after failed start, success update, uniqueness of registry/loader ownership, stopping, and audit evidence.
- Connected `make test` to the executable integration harness. Its test-only staging directory is private and isolated; production Core staging remains unchanged.
- **Limitations:** No host execution receipts yet, and concurrent in-flight command withdrawal and physical crash resilience are not covered. All code remains pending compile/test and operator qualification.

## 2026-10-27 — Executable Core unit behavior tests added (source committed, not run)

- Added `tests/test_core_behavior.c` with 20 actual C behavior cases for Digit Root validation and Core policy decisions, including negative and escalation cases, and isolated audit-log verification.
- Updated `make test` to build and run the C test binary alongside 20 source-contract checks. No production module or service is touched by the test executable.
- **Remaining:** hotload/rollback failure-injection, Core/module integration, routing concurrency and full revised-Core qualification. A passing 20-case unit run does not replace the complete applicable Core suite. Build and test have not been executed by this assistant.

## 2026-10-27 — Core regression entrypoint restored (not full qualification)

- Director-authorized correction: added `tests/test_core_contracts.py` with 20 independently named source-contract regression checks covering root, logging, qualification, authorization, hotload and rollback safeguards.
- Restored `make test` in the root Makefile to build Core and run these checks; created the previously missing `docs/CORE_QUALIFICATION.md`.
- **Important distinction:** source-contract checks are not executable Core behavioral qualification and do not satisfy the mandatory 20/20 requirement. Runtime test suites and failure-injection coverage are still missing, and no target-host build, test run or qualification evidence was collected here.

## 2026-10-27 — Protected Core hotload rollback correction (source committed; qualification pending)

- Director-authorized Core correction in `src/core_hotload.c`: candidate qualification remains isolated; the incumbent shared library is now retained through candidate activation. On candidate activation failure, Core attempts to restore the previously qualified incumbent descriptor and reactivate it, recording `HOTLOAD_ROLLBACK_ACTIVE` or `HOTLOAD_ROLLBACK_FAILED`. This is best-effort rollback, not proof of uninterrupted service.
- Qualification evidence now records candidate SHA, executed/passed/failed counts, negative-executed/negative-passed counts, and ABI result for new and updated modules; `src/logging.c` retains the evidence even on `QUALIFICATION_GREEN` in `/opt/digit/logs/modules.log`.
- **Limitations:** no target-host build or Core 20/20 test suite executed here. No rollback failure-injection test or deployed acceptance evidence has been established. Current source is NOT qualified for deployment without the required tests. Remaining concerns include candidate activation cleanup on partial failures, concurrent unload safety, and rollback failure scenarios.
- No shared ABI, IRC or Ollama source changed.

## 2026-10-27 — Live IRC routing evidence (operator observed; full qualification pending)

- **Module activation receipts:** IRC `1.1.0` activated at 16:32:15 UTC; Ollama `1.0.4` activated at 16:43:14 UTC.
- **Positive live routes:** `digicat: Hello` returned `Hello.`; `digicat: /chat Hello` returned `Greetings. What can I assist you with?`. This demonstrates a live IRC ingress → Core dispatch → registered chat controller → Ollama generation → IRC reply path.
- **Negative live route:** `digicat: /unknown test` produced `[IRC] Core route unavailable result=5`; ABI 1.5 defines result 5 as `STNLABZ_MODULE_ERR_NOT_FOUND`. An IRC-side error reply is not implemented; this test proves internal rejection, not user-facing failure feedback.
- **Timeout investigation:** `[OLLAMA] request failed curl=28 http=0` and route result 7 correlated with Ollama API timeouts. An independent 60-second localhost API request also timed out. Operator identified a `lesson-exporter` Python client connected to Ollama, elected to remove the competing service, and subsequently observed a successful Digit reply. No continuous-performance qualification or installed-service removal receipt was recorded.
- **Scope of evidence:** successful targeted live checks and activation receipts only. Not proof of a warning-free build, all unit/negative suites, concurrency/unload integrity, identity/authorization verification, deployment byte hashes, or Director's final milestone acceptance. Protected Core and ABI source unchanged by this documentation update.

## 2026-10-27 — Module installation integrity correction (source committed; live verification pending)

- Investigated operator evidence: installed Ollama `ollama.so` was `root:root` with an October 26 timestamp, while its SHA-256 differed from the newly built `modules/ollama/build/ollama.so`. Runtime evidence last confirmed Ollama 1.0.3, whereas repository source declares 1.0.4. Incorrect owner and different bytes are **separate observed facts**; ownership alone is not proven to have caused the stale installation.
- Corrected complete `modules/ollama/Makefile` and `modules/irc/Makefile` install targets to specify `digit:digit`, stage binaries in the destination directory, compare staged versus built bytes, rename into the watched path atomically, verify installed bytes and owner, and fail on a violated invariant.
- Kept the separately managed `modules/lesson_exporter` service's `lesson-exporter` account unchanged; its deployment is not a Digit hotload shared object. No protected Core or shared ABI source changed.
- **Verification pending:** operator privileged install, matching SHA-256 digests, `digit:digit` ownership, Ollama 1.0.4 activation and runtime chat-controller dispatch. Repository source changes do not install binaries.

## 2026-10-27 — IRC version doctrine correction and routing diagnostic (source committed; runtime pending)

- Corrected the IRC module descriptor from invalid **1.0.11** to **1.1.0**, because approved STN-LABZ version doctrine mandates **1.0.10 → 1.1.0**. The previously recorded 1.0.11 activation is a historical observation, not the new target version.
- Verified Ollama source currently declares **1.0.4** and registers its `chat` controller; the supplied operational logs last show **Ollama 1.0.3** active. A source declaration is not proof that 1.0.4 was built, installed or activated.
- IRC source and documentation version identities were aligned. No installed binaries or running modules were changed through GitHub updates.
- **Gate:** deploy and observe the corrected IRC 1.1.0 and Ollama 1.0.4 alongside the new Core Router, then verify default/explicit/unknown-route behaviors and capture the failure category where applicable.

## 2026-10-27 — OpenSSL 3 EVP SHA-256 hotload correction (source committed; verification pending)

- **Authorized protected Core change:** replaced deprecated `SHA256_Init`, `SHA256_Update`, and `SHA256_Final` in `src/core_hotload.c` with `EVP_DigestInit_ex`, `EVP_DigestUpdate`, and `EVP_DigestFinal_ex`.
- Preserved SHA-256 binary digest size, lowercase 64-character hexadecimal output, module candidate comparison, and existing private staging path protections.
- Checked all EVP operation results and expected digest length; failures during reading, hashing, writing, or file closure reject a candidate rather than accepting an incomplete snapshot. Retry interrupted read/write calls and remove failed staged snapshots.
- No public ABI layout, module contract, transport, or hotload lifecycle transition changes were intended.
- **Evidence:** GitHub source change committed; deprecation calls removed from this Core file by source inspection. The target-host build, zero-warning compiler output, regression tests, hotload replacement tests, deployment, and operator acceptance remain **unverified**. Do not mark the change qualified until those receipts exist.

## 2026-10-27 — Core routing handoff (source committed; qualification pending)

- Added `include/digit_route.h` for transport-neutral request and reply data, with the Core-owned `digit.router.dispatch` service name.
- Updated `src/module_host.c` so the Core host resolves registered command destinations and invokes module controllers, with in-flight protection on command withdrawal.
- Updated IRC to submit addressed input to the Core-owned dispatch service instead of invoking the Ollama generation service directly. IRC source version: **1.1.0** (corrected from noncompliant 1.0.11).
- Updated Ollama to register a `chat` controller via existing ABI host callbacks. Ollama source version: **1.0.4**.
- The current Core ingress selects explicitly addressed routes using `/name arguments`; ordinary input selects the registered `chat` controller. This is current implementation behavior, not authorization to require users to enter commands for passive module work.
- Kept the shared STN-LABZ ABI **1.5** host structure unchanged. No changes were made in `stnlabz/ABI`.
- **Evidence status:** source commits exist; no recorded build, qualified regression test, synchronized installation, end-to-end IRC round trip, or operator acceptance for this routing change.
- **Open risks:** verify route availability during module stop/reload, actual identity/authority handling, 512-byte ABI command argument limit, route naming, error delivery, reply size, and that modules own their internal operations. No S2/Research/Publish route acceptance is claimed.

## 2026-10-27 — Digit identity corrections (operator installed previous revision)

- Replaced legacy Rictus identifiers with Digit identifiers across the active Core host, interface headers, IRC module, Ollama module, module tests, and module READMEs.
- Aligned the named service contracts, including `rictus.llm.generate` → `digit.llm.generate`, and the transport-send services.
- Preserved historical lesson records and historical Rictus observation references.
- **Operator report:** changes were built and installed, Digit Core was restarted, and Digit was back in IRC. This observation applies to the identity-correction milestone and **must not** be carried forward as verification of the later routing changes.

## 2026-10-26 — IRC transport revisions (source history)

- **IRC 1.0.8:** categorized startup/network failure diagnostics without disclosing credentials or raw messages.
- **IRC 1.0.9:** one- or two-channel support, with independent channel tracking and reply destination behavior.
- **IRC 1.0.10:** IRCv3-tagged and nick-only-prefix PRIVMSG parsing, buffered TLS reads, and categorized ingress rejection diagnostics.
- These are documented source revisions; their build, live network, and operator-acceptance states must be supported by separate receipts.

## 2026-10-19 — Core qualification milestone (historical)

- The repository README records a **20/20 milestone**, **eight configured test suites**, and **41 positive / 31 negative assertions passed** in the operator-provided Core testing record.
- That evidence applies to the Core revision tested on that date; it is not retroactive proof of the 2026-10-27 routing changes.

## Continuity and change control

1. Record each change with scope, affected components, source revision, and whether it modifies the protected Core.
2. Keep **source committed**, **build verified**, **qualified**, **installed/activated**, **end-to-end verified**, and **operator accepted** as separate states.
3. Do not claim a milestone complete without evidence. Tests do not authorize deployment, follow-on implementation, or Core modifications.
4. Preserve the established ChAoS MVC boundaries: Core routes/manages; module controllers route internally; module models work; views are optional; transports communicate.
5. Correct demonstrated root causes and log resulting evidence. Changes to protected Core require direct Director authorization.
6. For future work, update this changelog and `docs/ROADMAP.md` together; do not fabricate results or rewrite history.

## Core self-unregistration guard

2026-10-27: Director-authorized Core fix: module callback self-unregistration now fails immediately instead of deadlocking on its own in-flight counter. Added 12 executable service/command assertions in tests/test_core_self_unregister.c; make test-reentrant is included in make test. Source committed; compilation and operator results pending. Existing 23/23 threaded tests must be re-run because Core changed.

## 2026-10-27 — Reentrant test count correction

- Operator execution reported 13/13 PASS, but the test executable returned failure because its final assertion incorrectly required exactly 12 checks.
- Corrected `tests/test_core_self_unregister.c` to require exactly 13 passing checks. No production Core logic changed. Corrected harness still requires a rerun for a successful exit-status receipt.

## Concurrent hotload with an in-flight callback

2026-10-27: Added `tests/test_core_hotload_inflight.c` with 15 real C integration assertions for concurrent replacement of a module whose service callback is blocked in flight. The fixture's `stop()` unregisters its service through Core; the test checks replacement waits, callback completion, loader and registry uniqueness, stale-route removal and successful replacement. Added `test-inflight` to mandatory `make test`. No production Core or ABI logic changed. New test has not been executed; host-side verification is required. This does not cover every shutdown implementation or crash scenario.

## S2 Sentinel collection milestone — 2026-10-27

2026-10-27 — Director-authorized S2 Cyber Security collection milestone: added native `modules/s2` v1.0.0. Source reads the documented Sentinel JSON `https://api.stn-labz.com/threats`, polls autonomously at 900-second intervals, tracks cases by source ID and content signature, atomically persists the register, and exposes `/s2` aggregate status. Source covers missing severity explicitly; it does not infer compromise. Includes ten internal qualification assertions plus an independent persistence test, and owner-verified staged installation. Dependencies: libcurl, json-c and pthreads. **Source only. Build/test, module qualification, installation, live Sentinel ingestion and operator acceptance NOT yet performed.** Full source evidence/event history, investigative correlation and Ollama-assisted narrative are not yet implemented; do not claim the full S2 mission is restored. No Core or shared ABI touched.

## 2026-10-27 — S2 collection cadence reduced

- Director requested Sentinel collection every four hours rather than every 15 minutes to minimize noise.
- Updated the S2 native module timer to 14400 seconds and module.conf accordingly; bumped module release from 1.0.0 to 1.0.1 so Core can hotload the revision.
- S2 polling does not independently post to IRC; /s2 reporting remains on-demand. This change does not create unsolicited IRC announcements.
- Source committed; revised module qualification, installation and operator verification pending. Core and shared ABI unchanged.

## 2026-10-27 — S2 v1.0.2 IRC scheduled synopsis

- S2 now submits a short baseline synopsis to the registered `digit.transport.channel.send` service after its first successful Sentinel collection, then an update only when a scheduled four-hour collection detects new or revised source records. Unchanged polls remain silent; no direct IRC socket or Publisher dependency.
- The command `/s2` remains available. Source findings are identified as leads rather than confirmed incidents. Delivery failure logs and does not block evidence collection.
- Operator build, module qualification, hotload, and IRC client verification are pending for v1.0.2; no Core or shared ABI changes.

## 2026-10-27 — S2 v1.0.3 readable collection time

- Corrected `/s2` and `/s2 status` to format the last successful Sentinel collection as UTC (`YYYY-MM-DD HH:MM:SS UTC`) using thread-safe `gmtime_r`; displays `never` when no successful collection exists. No changes to the four-hour poll cadence or automatic IRC noise controls.
- Revision committed only; compilation, test qualification and installation remain operator-controlled and unverified for v1.0.3.

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

## 2026-10-28 — Identity v1.1.1 grouped-question parser test correction

- Operator's `make -C modules/identity clean all test` compiled and linked, with embedded self-qualification 13/13 and negative 1/1, but standalone test returned FAIL.
- Cause: the added standalone positive case `what are your general orders?` was not normalized by `identity_question_route`; the parser only stripped `what is` and similar introductions. Corrected the complete `modules/identity/src/identity_model.c` file to recognize `what are ` so the grouped order question resolves to `orders`.
- No Core, IRC, Ollama, ABI, or General Order contents changed. Re-run pending; PASS is not claimed.

## 2026-10-28 — Identity v1.1.2 response presentation

- Director requested natural first-person responses for General Order questions. Identity now renders each approved order as an `I must` statement, with the original data unchanged.
- Core, IRC, Ollama and ABI unchanged. Source committed; tests and live verification remain pending.


## 2026-10-28 — Director-authorized Rictus DSD and Ollama transfer to Digit

- Ported the proven Rictus Core DSD diagnostic foundation into Digit Core using Digit-specific naming and source paths.
- Added bounded in-memory last-request traces per sender/account. Core records the resolved route, registered command owner, command result, reply state, nested service invocations, service owners/results, and a repair-oriented disposition.
- Added Core registration ownership attribution during module activation so diagnostics derive module ownership from the actual runtime registration path rather than a static module map.
- Added natural Core diagnostic control request `diagnose my last request`, evaluated before starting a new trace so the diagnostic request does not overwrite the request being inspected.
- Added dynamic source correlation against the current Digit tree. Default source root is `/home/stnchain/digit`, overridable with `DIGIT_SOURCE_ROOT`; reported locations are repo-relative `Code: file:line` evidence when correlation is provable.
- Updated Core qualification link sets so tests that compile `module_host.c` or `core_activation.c` also link the new diagnostics/ownership dependencies.
- Shared ABI 1.5 is unchanged. No SQLite persistence was introduced.
- Ported the proven Rictus peer-address/action behavior into Digit Ollama and advanced the module from **1.0.5 to 1.0.6**. Digit's authoritative Identity-service delegation remains first in the `chat` controller; only requests that fall through to generation use the peer-action path.
- Ollama 1.0.6 recognizes the currently proven `say ... to <nick>`, `tell <nick> ...`, and `ask <nick> ...` forms, preserves the exact peer nickname, preserves question intent for `ask`, and sends peer-directed output through Core's existing `send_message` boundary instead of forcing the result back to the conversation origin.
- Source committed to GitHub only. Core clean build, full `make test`, Ollama build/selftests, installation, activation, end-to-end Digit client verification, and Director acceptance remain pending.

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

## 2026-10-29 — S2 v1.2.0 operator build correction

- Operator build halted because `add_reader_note` was called before its static declaration. Added the forward declaration.
- Corrected the HTTP:BL public-report `snprintf` placeholder for the prepared research summary, eliminating the extra-format-argument warning and restoring its inclusion in public text.
- Fixed incorrectly escaped newline and carriage-return character constants in HTTP:BL key parsing.
- Removed dead IRC synopsis code and an unused report counter to address reported warnings, without modifying Core or IRC; aligned public investigative headings with test assertions.
- GitHub source corrected; a clean operator build and qualification are still required. No claim of production success.

## 2026-10-30 — S2 operator-triggered live sai-dsa publishing test

- Added native C `modules/s2/tests/test_sai_dsa_live.c` and explicit `make -C modules/s2 test-live-page` target; separate from automatic `make test` and normal hotload operation.
- Intended action: prove the service can read the OS-protected website credential file, obtain login CSRF, authenticate, locate the actual `sai-dsa` edit form, preserve the existing Markdown and page metadata, append a timestamped test message, submit it, then verify exact content on read-back.
- Diagnostic messages name the failing stage without displaying secrets. Explicit operator invocation is required because the test makes a visible live-page edit.
- **Verification:** GitHub source committed. No claim that the live site was edited, or that this host-only integration target has compiled or passed. Normal S2 publishing remains unverified.

## 2026-10-30 — S2 Pages diagnostic and HTTP method correction

- Operator confirmed the protected login credential file was readable when running as `digit`, and an independent GET of `https://stn-labz.com/auth/login` returned HTTP 200 with a CSRF token in the expected HTML form.
- Corrected S2's libcurl GET setup: clear POST payload first and set `CURLOPT_HTTPGET` last; the former ordering could change GET back to POST. Native publisher and explicit live-page test share this corrected helper.
- Added nonsecret HTTP status/byte-count diagnostics and separated login GET, login CSRF, authenticated Pages GET and page form checks. Redirect status is treated as expected for authenticated POST.
- Removed `snprintf` truncation warning in the live test using a verified-length append.
- **Pending:** The operator must rebuild and rerun the manual live-page target as the `digit` service user. No claim of successful authentication or public page update.
