# Core Migration Record — First Foundation

Source: [stnlabz/digit](https://github.com/stnlabz/digit) (default branch read on 2026-10-12).  
Destination: [poemei/digit](https://github.com/poemei/digit).

## Imported unchanged

- `include/agent_root.h` — Core Root descriptor and validation contract
- `src/agent_root.c` — Root identity and General Orders validation
- `src/identity_digit.c` — **Digit's compiled identity and foundational traits**, including her three General Orders
- `tests/test_agent_root.c` — existing Root qualification tests
- `include/core_sa.h`, `src/core_sa.c` — fail-closed Core-owned SA verification
- `tests/test_core_sa.c` — existing SA negative tests
- `include/digit.h` — existing Core name and entry-point declaration (entry point **not implemented** in this first foundation)

The imported files are source-reference copies, not proof of qualification in the new repository. `Makefile` is a minimal standalone test harness for the transferred components.

## Intentionally not imported yet

The old Core includes `src/digit.c`, `src/runtime.c`, `src/module_manager.c`, `src/hotload.c`, `src/service_registry.c`, `src/qualification.c`, `src/authority.c`, `src/audit.c`, `src/alert.c`, `src/channel.c`, and related headers. These require inspection of dependency contracts and Core-only responsibility before migration. In particular, the old `src/digit.c` assumes a full module/runtime integration; importing it immediately would recreate unreviewed coupling.

No `modules/` directory, Dispatcher, Interface, OAuth, ChatGPT connector, conversational pipeline, Rendering, or DSD code has been migrated.

## Qualification boundary

This first commit set is **Core identity/authority groundwork only**, not Phase 1 Core acceptance. Run:

```sh
make clean
make
make test
```

Core startup without modules, standalone policy enforcement, module qualification, and lifecycle admission remain outstanding roadmap gates. The new Core must satisfy them before Dispatcher development.

**Development rule:** Start with Core and keep her identity and core functions there. Every other capability belongs outside Core.
