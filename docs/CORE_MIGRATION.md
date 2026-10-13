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
