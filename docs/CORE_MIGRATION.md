# Core Migration Record — First Foundation

Source: [stnlabz/digit](https://github.com/stnlabz/digit) (default branch read on 2026-10-12).  
Destination: [poemei/digit](https://github.com/poemei/digit).

## Imported unchanged

- `include/agent_root.h` — Core Root descriptor and validation contract
- `src/agent_root.c` — Root identity and General Orders validation
- `src/identity_digit.c` — **Digit's compiled identity and foundational traits**, including her three General Orders
- `tests/test_agent_root.c` — existing Root qualification tests
- `include/digit.h` — existing Core name and entry-point declaration (entry point **not implemented** in this first foundation)

The imported files are source-reference copies, not proof of qualification in the new repository. `Makefile` is a minimal standalone test harness for the transferred components.

## Scope correction: SA belongs in a module

SA-specific verification and SA qualification are **not Core requirements**. The previously copied `include/core_sa.h`, `src/core_sa.c`, and `tests/test_core_sa.c` have been removed from this repository's Core, and their Makefile targets removed. SA functionality may be developed later as a separate module after Core qualification. Digit's foundational identity, General Orders, and Core-only boot must not depend on the presence of an SA module.

## Intentionally not imported yet

The old Core includes `src/digit.c`, `src/runtime.c`, `src/module_manager.c`, `src/hotload.c`, `src/service_registry.c`, `src/qualification.c`, `src/authority.c`, `src/audit.c`, `src/alert.c`, `src/channel.c`, and related headers. These require inspection of dependency contracts and Core-only responsibility before migration. In particular, the old `src/digit.c` assumes a full module/runtime integration; importing it immediately would recreate unreviewed coupling.

No `modules/` directory, Dispatcher, Interface, OAuth, ChatGPT connector, conversational pipeline, Rendering, or DSD code has been migrated.

## Qualification boundary

This first migration stage is **Core identity groundwork only**, not Phase 1 Core acceptance. Run:

```sh
make clean
make
make test
```

Core startup without modules, standalone policy enforcement, module qualification, and lifecycle admission remain outstanding roadmap gates. The new Core must satisfy them before Dispatcher development.

**Development rule:** Start with Core and keep her identity and core functions there. Every other capability belongs outside Core.


## Core-only lifecycle migration (2026-10-12)

The original `src/runtime.c` bound its run loop to `hotload.h` and `module_manager.h`. This rebuild **retains the existing Core operational-state semantics** (normal, Core fault → Company Preservation, and explicit human-release flag), but removes module/hotload dependencies from the runtime itself.

New Core-only implementation:

- `include/runtime.h`, `src/runtime.c`: independent lifecycle, signal-based shutdown, and Core fault-state handling.
- `src/digit.c`, `src/main.c`: standalone entry point; validates Digit's compiled Root before running.
- `tests/test_runtime.c`: positive and negative tests of operational-state transitions.
- `Makefile`: builds `bin/digit` and both Core component test binaries without external modules.

This is a **targeted adaptation**, not a verbatim runtime copy. It does not migrate the old module manager, hotload polling, audit/channel/alert services, or SA functions. In particular, `digit_runtime_human_release(..., authorized_human_release)` consumes an authorization outcome; **it is not itself an identity verification mechanism**. Real authorization integration is subject to a separate Core boundary review.

Build and test:

```sh
make clean && make && make test
```

Launch independently using `./bin/digit`; use SIGINT (Ctrl-C) or SIGTERM for graceful shutdown. **Compilation, unit-test results, and live runtime behavior must be reported from execution**, not inferred from the GitHub commits.

Core Phase 1 is **not yet completely qualified**: module ABI, registry/admission and qualification boundaries, startup error handling, and zero-module lifecycle behavior still require review and acceptance.


## Core-only module-management foundation (2026-10-12)

Added `include/module_manager.h`, `src/module_manager.c`, and `tests/test_module_manager.c`. These files implement **Core infrastructure**, not a functional module: a versioned ABI descriptor, bounded registry, qualification evidence gate, explicitly authorized activation, start/stop callbacks, and shutdown. No module is auto-created or auto-started. Core still runs with **zero modules**.

This is deliberately a **small fresh Core-only contract** informed by the older repository, not a wholesale copy of its module loader, discovery, hotload, or application-service routing.

**Current limitations:** Registration is in-process only. No shared-library discovery/loading, signature checking, persisted qualification records, or external transport is implemented. The `operator_authorized` value supplied to `digit_module_authorize` is a decision **input**, not proof of actor identity; the verified Core authority boundary must supply it before production activation. The minimal qualification test count in this foundation is a temporary contract value pending alignment with the approved STN-LABZ qualification requirements. These limitations prevent a claim of full Core Phase 1 acceptance.

Run `make clean && make && make test` on the target environment. The new module-manager test uses an internal mock callback only; **no functional modules were migrated**. No build or runtime test is claimed from repository edits alone.

## Core startup integration (2026-10-12)

The standalone `bin/digit` executable now **initializes its Core module manager** after validating Digit's Agent Root, reports its zero-module registration count, and shuts down the manager on normal runtime exit and runtime failure. `Makefile` now links the manager into the Core executable.

**No module loading, implicit module activation, Dispatcher, transport, SA, or unrelated module has been introduced.** Core identity and the existing runtime remain independently operable when no modules are installed.

This integration does not establish production activation authority, persisted qualification, or dynamic discovery. Existing unit tests cover the independent manager. **The operator must run `make clean && make && make test`, then `./bin/digit` followed by Ctrl+C** to verify integrated startup/shutdown on the target system. Do not mark Phase 1 complete from a successful commit alone.

## Mandatory Core logging (2026-10-12)

The installation root is **`/opt/digit`** and the sole canonical log file is **`/opt/digit/logs/digit.log`**, not `audit.log`.

- Added `include/logging.h`, `src/logging.c`, and `tests/test_logging.c`.
- `bin/digit` opens the log before Root validation and refuses startup if logging cannot be opened or key startup events cannot be recorded/synced.
- Core records startup, Agent Root validation, module-manager initialization, running, runtime failures, and shutdown through the common logger.
- The log directory must exist and be writable by Digit's service identity **before starting**. Core does not silently create or grant permissions on installation directories.
- `digit_log_open_path()` permits a private test file; production uses only `digit_log_open()` and the canonical pathname.
- The logging facility is Core-owned infrastructure; functional modules must eventually submit their operational events through it rather than write independent `audit.log` files.

**Still to implement before logging is fully universal:** Core module-manager event instrumentation, runtime transition logging, and module ABI access to the Core logger. The new logging test proves logging format/path behavior; it does not alone prove that every future activity is logged.

To qualify on the installed host, ensure `/opt/digit/logs` exists and is writable by the service account, then run `make clean && make && make test && ./bin/digit`. Check `/opt/digit/logs/digit.log` for structured Core startup/shutdown records. No build or runtime qualification is claimed solely from these commits.

## Local logging path adjustment (2026-10-12)

Production Core logging uses `./logs/digit.log` relative to Digit's **current working directory**. On launch from `~/digit`, the log is `~/digit/logs/digit.log`. `digit_log_open()` creates `./logs` with mode 0700 if absent; Core still refuses startup when it cannot open its mandatory logger. `/opt/digit/logs` is no longer required for this development build. When installed under `/opt/digit`, launch with working directory `/opt/digit` to store logs there. No alternate audit.log is used.

## Current operator-selected Core acceptance evidence (2026-10-12)

The operator has selected the Core build, `make test` aggregate pass/fail output, standalone console startup/shutdown, and local `./logs/digit.log` records as the current foundation-stage evidence. There is no new Windows APP or Slack connection for this rebuilt Digit. Successful automated checks are not operator acceptance; stop and await explicit acceptance plus next named step authorization under `AGENTS.md`.

The canonical **current development** logging location is `./logs/digit.log` relative to the process working directory; earlier historical `/opt/digit/logs` instructions in this record have been superseded by the local logging adjustment above. Router (formerly called Dispatcher in legacy material) is not yet implemented, and Bootstrap/Router names and responsibilities follow the current roadmap. SA remains outside Core.

## Core event logging instrumentation (2026-10-12)

**Authorized scope:** extend the existing Core logger to record module-manager operations and runtime state changes. No functional modules, Router, Bootstrap, SA module, or transport has been added.

- `src/module_manager.c` records manager initialization/shutdown and registration, qualification, authorization, activation, rejection, stop, and shutdown-stop outcomes through `digit_log_event`, including the module identifier and result code.
- `src/runtime.c` records initialization, runtime entry/exit, Core fault/preservation transition and authorized human-release outcomes.
- `tests/test_module_manager.c` now opens a private log and asserts that key registration, rejection, activation, and stop records are actually present.
- `Makefile` links `src/logging.c` to runtime and module-manager component test executables.

**Limitations:** An external actor-verification mechanism for authorization has not been implemented. Test runners use a private log rather than claiming production activation. Some invalid/null argument paths cannot safely supply an identifiable module and may not emit diagnostic records. The module-management interface is still an in-process foundation only. The source and tests have been committed but have **not** yet been compiled or run on the operator's host.

**Required operator check:** `git pull && make clean && make && make test`, then `./bin/digit`, stop with Ctrl+C, and inspect `tail -n 30 logs/digit.log` for Core and Runtime entries. The operator reviews the aggregate test report and runtime log before deciding acceptance under `AGENTS.md`. This work does not authorize a next development step.

## Core qualification and admission hardening (2026-10-12)

**Authorized step:** reinforce Core qualification and admission checks without adding any functional module, Router, Bootstrap, or client.

- `src/module_manager.c` now checks that positive tests were executed and all passed, failed tests are zero, and both negative-test fields are exactly `1` before moving a registered module to QUALIFIED.
- The same qualification evidence is required again at authorization and activation, rather than relying solely on the module state value.
- The QUALIFIED transition occurs only after the associated qualification result has been successfully logged.
- `tests/test_module_manager.c` adds negative evidence and lifecycle checks, including denial of requalification when ACTIVE, required authorization after STOPPED, and authorized restart.

**Limits:** these checks validate the supplied evidence structure, **not its authenticity**; verified actor authorization, qualification persistence, and dynamic module loading are still outstanding. `DIGIT_MODULE_MIN_TESTS` remains unchanged in this step. Source changes are committed; compilation, test success, and operator acceptance have not yet been established for this revision.

**Operator procedure:** run `git pull && make clean && make && make test`, confirm aggregate `OVERALL RESULT: PASS` and zero suite/report errors; run `./bin/digit`, stop it with Ctrl+C, and inspect `tail -n 20 logs/digit.log`. Expected: module-manager tests pass including the added negative and restart checks; Digit still starts with zero registered modules and emits startup/stop events. No functional module activation should occur in the running Core.

## Shutdown log verification regression (2026-10-12)

An operator reported shutdown missing from `digit.log` despite clean console exit. A prior log excerpt contained a complete shutdown sequence from an earlier run, but did not establish the result of the newer run. Source inspection found that the current normal shutdown path already writes and synchronizes `RUNTIME STOP` and `CORE STOPPED`; the reported failure's precise cause is not confirmed.

- New `tests/test_shutdown.sh` runs `bin/digit` in a fresh temporary working directory, waits for its logged running state, requests SIGTERM, waits for successful exit, and checks the **same run's** `RUNTIME STOP`, `MODULE MANAGER_SHUTDOWN`, and `CORE STOPPED` records; also rejects duplicate Core stop entries.
- `tests/run_all.sh` includes this fifth integration suite in `make test` and treats a missing record or unexpected exit as a test failure.
- `src/digit.c` displays the log path in the successful shutdown confirmation, only after shutdown records are written and synchronized.

This is a regression guard and improved verification, not a claim that the historical failure's cause has been reproduced. Changes committed to GitHub only; no operator-host compilation or runtime results are asserted. Operator: `git pull && make clean && make && make test`, then `./bin/digit`, Ctrl+C, and `tail -n 20 logs/digit.log`. The fifth suite must pass and the current invocation must have its own shutdown records. Await explicit operator acceptance.

## Qualification-integrity audit gate (2026-10-12)

Authorized Core-only step: review qualification/admission evidence trust and prevent admission without the mandatory audit trail.

**Finding:** `digit_module_qualification_t` contains counters and two boolean indicators. These are caller-supplied assertions, **not independently verifiable qualification evidence**. The current public ABI provides no authenticated provenance or trusted test-result receipt. No claim of independently verified qualification is made, and no new ABI, authority provider, functional module, or qualification threshold is introduced here.

**Change:** `src/module_manager.c` now denies registration, qualification, and authorization when the Core logger is not open or the decision cannot be synchronized. Registration records its decision **before** inserting the record into the registry; failed auditing leaves the registration count unchanged. Activation and stop callbacks require an audited attempt record before execution. Existing state/result types are preserved.

**Tests:** `tests/test_module_manager.c` asserts that registration without the logger is rejected with no registry change, and that registration and authorization are rejected after logging is closed. Previous qualification negative tests remain.

**Open risk:** caller-supplied test counts can still be fabricated by a trusted or compromised caller; authenticating them requires a separately approved trusted evidence source/contract. This step addresses audit integrity, not evidence provenance. Production authority identity and persistent qualification verification remain outside this change.

**Operator gate:** `git pull && make clean && make && make test`; check the fifth shutdown integration suite, updated module-manager suite, and overall PASS. Run `./bin/digit`, stop with Ctrl+C and confirm the shutdown events in `tail -n 20 logs/digit.log`. Report failures exactly; no passing server test or acceptance is presumed from commits.

## Shared STN-LABZ ABI 1.5 Core integration (2026-10-12)

**Operator-authorized compatibility stage.** Reference: `stnlabz/ABI` default branch as read for this integration. The governing common module contract is ABI **1.5** (`STNLABZ_MODULE_API_MAJOR=1`, `STNLABZ_MODULE_API_MINOR=5`, `STNLABZ_MODULE_MIN_TESTS=10`). These shared ABI sources were copied **unchanged** to `include/stnlabz_abi/` and `src/stnlabz_abi/`; the upstream repository was not altered.

- `src/digit.c` initializes the common ABI's `stnlabz_module_registry_t` for **zero-module Core startup**. The prior Digit-local `digit_module_manager_t` is no longer linked into `bin/digit`.
- `Makefile` links the shared lifecycle implementation (`module.c`, `module_registry.c`, `abi.c`) to Core and adds a dedicated compatibility test binary.
- `tests/test_abi_compat.c` covers ABI version/minimum test count, descriptor qualification callback, denied unauthorized activation, authorized start, stop and cleared authority, unregister/replacement, fresh qualification, rejected insufficient tests, and incompatible ABI minor.
- `tests/run_all.sh` includes the compatibility suite alongside previous Core and shutdown suites.
- The historical standalone Digit-local manager and its tests remain as migration comparison material, **not a second active runtime lifecycle controller**. A later explicit operator-authorized cleanup decision is needed before deleting or replacing those tests and files.

**Limits:** no dynamic loader, module discovery, or hot replacement filesystem watcher is enabled. The shared ABI registry currently has no live modules. Common ABI qualification callbacks return evidence supplied by module code and do not by themselves attest binary provenance or human approval. Production actor authentication, secure evidence provenance, and an audited activation boundary remain separate work. The exact descriptor export required by the established loader is `stnlabz_module_get_descriptor`; it has not been changed.

**Operator acceptance:** run `git pull && make clean && make && make test`, verify **6** suites and aggregate PASS, then launch `./bin/digit`, stop with Ctrl+C, and inspect `tail -n 20 logs/digit.log`. Expect zero modules and shutdown records `RUNTIME STOP`, `MODULE MANAGER_SHUTDOWN`, and `CORE STOPPED`. The ABI compatibility suite runs mock callbacks only; no real module is activated. Do not infer host test success from GitHub commits.

## ABI 1.5 Linux loader compatibility milestone

**Operator-authorized Core-only change.** Imported `stnlabz/ABI` `includes/module_loader.h` and `src/loader_linux.c` unchanged into Digit's vendored ABI 1.5 tree. No shared ABI edits.

Digit Core initializes an empty Linux loader alongside its empty ABI registry, and links `-ldl`. **No automatic module scanning, discovery, qualification, activation, or hotloading.** Existing Core zero-module startup and logging behavior remain the operational baseline. This loader is a handle mechanism, not an authority or lifecycle controller. Host integration must never unload active module code: a future authorized admission/unload path must enforce stop and quiescence and record its actual transitions before unloading.

A new `tests/test_loader.c` suite exercises a compiled mock `.so`: valid ABI descriptor export, expected identity, duplicate-load denial, clean unload, identity mismatch denial, legacy/missing-export denial, missing library denial, invalid identity, and missing-unload denial. The test verifies that loader success never registers or activates a module. `tests/fixtures/abi_loader_fixture.c` is test-only and not an operational functional module.

**Acceptance procedure:** `git pull && make clean && make && make test` should compile the loader and mock shared objects and complete **7** suites without failure; then start `./bin/digit`, stop using Ctrl+C, and inspect `tail -n 20 logs/digit.log`. Confirm zero registered modules and full shutdown log. Automated tests establish Linux mock loader contract behavior only; acceptance and live real-binary compatibility remain separate. No source-only claim of passing server tests.

## Core-controlled discovery, ABI 1.5 (2026-10-12)

**Authorized scope:** Discovery only. Vendored `stnlabz/ABI` `module_discovery.h` and `module_discovery_linux.c` unchanged. No ABI public layout, entry point, qualification threshold, or loader contract change.

At Core initialization, the approved Linux discovery path is obtained from the executable location (`bin/digit` looks for `bin/modules`). For a candidate directory, shared discovery expects `module.conf` with `id=...` and `bin/<id>.so`. It loads the descriptor and records `DISCOVERED` in the shared Core registry; it does **not** verify, qualify, authorize, or activate it. A missing modules directory is normal and recorded as `DISCOVERY SCAN_ABSENT`.

Digit's thin `core_discovery` host wrapper records scan counts and the last rejected module's actual stage/reason in `./logs/digit.log`. **Known limitation:** the upstream discovery report only holds the most recent rejection, not an individual audit entry for every candidate when multiple modules are rejected in one scan. No claim of complete per-candidate audit is made. The shared discovery scanner also reports rejection details to stderr.

On shutdown, Core transitions discovered-only candidates out of the registry before unloading their handles; no active module unloading, dynamic replacement, monitoring, or module execution is enabled. Module fixtures are limited to tests.

**Operator tests:** `git pull && make clean && make && make test` should execute eight suites, including discovery manifest and shared-library positive/negative cases. Run `./bin/digit`, Ctrl+C, then `tail -n 25 logs/digit.log`; inspect `DISCOVERY SCAN_START`, `SCAN_ABSENT` when `bin/modules` is absent, Core startup with zero modules, and completed shutdown. To test discovered candidates, use only test fixtures; do not place untrusted shared libraries under the operational scan tree. Build/test and human acceptance remain separate gates.

## ABI 1.5 descriptor verification gate (2026-10-12)

**Authorized Core-only step:** after discovery, invoke the **existing** `stnlabz_module_registry_verify` for each discovered descriptor. The unmodified shared ABI 1.5 contract checks `required_core_api_major == STNLABZ_MODULE_API_MAJOR` and `required_core_api_minor <= STNLABZ_MODULE_API_MINOR`. Successful candidates transition `DISCOVERED → UNVERIFIED`; incompatible candidates transition to `FAILED`. This is **descriptor compatibility checking, not binary authenticity or publisher attestation**.

Digit's host wrapper logs each verification attempt (candidate identity, semantic version and required ABI) and outcome (result and observed state) in `./logs/digit.log`. A failed candidate does not prevent verification of other candidates; a failed mandatory audit operation stops Core startup. Cleanup now safely removes DISCOVERED, UNVERIFIED and FAILED candidates from the registry before unloading their handles. No qualification callback, start callback, authority assignment, or activation is invoked.

**Security limitation:** Linux `dlopen()` may execute library constructors during discovery **before** verification; this milestone does not provide a secure untrusted-code sandbox or cryptographic signature verification. Do not place untrusted binaries in the operational module directory. Operator approval and qualification remain separate.

**Test:** `git pull && make clean && make && make test`. The discovery suite now includes a valid descriptor's UNVERIFIED/no-authority state, incompatible-major FAILED state, non-blocking rejection, verified and rejected audit evidence, and cleanup. Expect eight suites. Run `./bin/digit`, stop with Ctrl+C and inspect `tail -n 25 logs/digit.log` for discovery, shutdown and, where candidates exist, per-candidate verification. With no modules directory, `SCAN_ABSENT` remains normal. Automated passing checks do not imply operator acceptance.


## Dynamic module capacity prerequisite (operator-authorized)

The fixed 32-entry caps in the upstream STN-LABZ ABI module registry, platform loaders, catalog, and qualification inventory have been replaced with dynamically growing containers. Digit vendors the updated upstream registry and Linux loader unchanged. Module ABI 1.5 descriptor layout, export symbol, lifecycle operations, and qualification threshold remain intact, but the **Core host container structures change layout** and must be recompiled as a matched revision.

Digit adds regression suites `registry_growth` and `loader_growth`. These exercise 128 registrations, multiple expansions, qualification/authority retention, duplicate denial, removal/reuse, injected registry allocation failure/recovery, and 65 distinct Linux mock shared libraries including loader allocation-failure injection/retry. No production module qualification or activation is enabled.

Run `git pull && make clean && make && make test` and inspect the reported suite/assertion totals. Test `./bin/digit`, Ctrl+C, and `tail -n 25 logs/digit.log`. Confirm zero-module discovery, runtime continuity, and complete shutdown logging. Upstream Windows loader, catalog, and inventory tests/builds are **not** verified by Digit's Linux suite and remain independently pending. Do not claim unlimited physical memory or full multi-platform qualification.

## Core-controlled module qualification (operator-authorized)

After shared ABI 1.5 discovery and descriptor verification, Digit's Core now invokes `stnlabz_module_registry_qualify` for each candidate in `UNVERIFIED`. The unchanged authoritative qualification contract enforces at least 10 executed tests, all tests passed, zero failures, and both negative-test-executed and negative-test-passed evidence. Qualification callbacks run candidate code: only deploy trusted shared libraries; this is **not** a sandbox.

The host reports each `QUALIFY_ATTEMPT`, `QUALIFY_PASS` or `QUALIFY_REJECT` into `./logs/digit.log`, including identity, result, lifecycle state, count fields and negative-validation results. Each candidate failure is isolated; logging failure stops Core. Successfully qualified modules remain **inactive and activation-unauthorized**. No start callbacks, automatic activation, Router, Bootstrap, or hotloading have been implemented. Shutdown safely unregisters qualified (non-active) modules and unloads their libraries.

The `qualification` test suite covers 64 verified mock descriptors (beyond the historical 32-entry cap), retained reports and authorization gates, repeat-call exclusion, rejection with fewer than 10 tests, missing negative-test success, failed test evidence and callback failure, plus per-candidate audit records. It does not claim to demonstrate security isolation or production module trust.

Operator validation: `git pull && make clean && make && make test` (expected **11 suites**, zero failures), `./bin/digit`, Ctrl+C, and `tail -n 25 logs/digit.log`. Without installed modules, `SCAN_ABSENT` and zero registered modules remain expected. Actual server compilation, runtime results and operator acceptance are separate gates.
