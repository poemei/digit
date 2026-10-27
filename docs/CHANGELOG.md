# Digit — CHANGELOG

This is the source-change record for Digit. Dates are source dates, not deployment or acceptance dates. Module versions are independent of Digit's agent release. A GitHub commit, successful compilation, or a module reconnect does not establish qualification or operator acceptance.

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
