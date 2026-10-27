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
