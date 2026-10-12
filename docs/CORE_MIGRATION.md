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
