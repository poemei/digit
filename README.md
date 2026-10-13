# Digit — Architectural Rebuild

Digit is an STN-LABZ modular autonomous-agent project. This repository is a **clean architectural rebuild** of Digit, using the established ChAoS MVC design as its structural reference.

The existing [stnlabz/digit](https://github.com/stnlabz/digit) repository is the **reference implementation**: it provides source material and previously developed functionality, not an architecture to copy wholesale. Working features may be migrated only after being realigned with the design and qualification requirements here.

## Mandatory development stop

**Module activation is not operator acceptance.** Build/test success, qualification GREEN and HOTLOAD_ACTIVE do not authorize moving to the next step. Follow [the operator acceptance gate in AGENTS.md](AGENTS.md): hand over the exact operator-selected test procedure, stop for the operator's Core build/test/console/log evaluation (APP/Slack testing once available), and wait for explicit acceptance plus authorization of the next named step. This applies to every rebuild phase.

## Linux installation — /opt/digit

Run `make clean && make && make test` first. Then install the Core executable and **all independently buildable modules**:

```sh
sudo make install
```

The installer creates the Linux system account/group `digit:digit` when absent. All installed files and directories under `/opt/digit` are recursively owned by `digit:digit`. Existing module directories and logs are preserved; the executable and installed module files are updated. Core's existing module discovery and logging code are unchanged.

```text
/opt/digit/
  digit                 # executable
  run                   # launcher (sets /opt/digit as working directory)
  logs/digit.log         # mandatory operational log
  modules/
    dsd/
      module.conf
      bin/dsd.so
```

Launch with `sudo -u digit /opt/digit/run`, or `sudo -u digit /opt/digit/run --mvc` for the previously accepted local test mode. **Use the launcher:** the current log contract is relative to the working directory, so invoking `/opt/digit/digit` directly from elsewhere would not guarantee `/opt/digit/logs/digit.log`. The executable-relative loader discovers `/opt/digit/modules/*/module.conf`, then each module's `bin/<id>.so`.

Every module remains independently buildable. To build/install only DSD: `make -C modules/dsd` and `sudo make -C modules/dsd install`. The root install target delegates to each present `modules/*/module.conf` module Makefile. Discovery and qualification do not imply activation. No Core source or ABI changes are required to add a module.

## Governing architecture

**Core → Bootstrap → Router → relevant Module → Rendering**

- **Core** is the foundation. It owns module lifecycle, registration, service contracts, qualification gates, and the infrastructure that enforces established policies and authority boundaries. Build and verify Core first; expand outward.
- **Bootstrap** initializes the application and hands off to Router; it does not run capabilities.
- **Router** is the single MVC routing component. It maps a request such as `/ask` directly to its registered Ask module. It does not interpret, compose, validate answers, or run a processing pipeline.
- **Modules** perform their own bounded jobs and return structured results to Router. Knowledge, language interpretation, diagnostics, validation, and other capabilities belong in modules according to their contracts—not as mandatory successive routing checkpoints for every request.
- **Rendering** provides the **view**: it presents results and diagnostic reports. Presentation must not become another source of business logic or routing authority.

This is **MVC in the ChAoS MVC pattern**: Router is the Controller; capability modules are Models; Rendering supplies the Views. Core provides the common foundation.

### Request flow

1. An interface receives a request and passes along verified transport/session context.
2. Core's established authority and policy controls remain applicable; Bootstrap passes control to Router, which selects the single responsible route/module (subject to Core policy).
3. Each invoked module does its assigned work and returns a bounded result, including evidence of its decisions or failures.
4. The responsible module returns its result for Rendering to present; Router does not do the module's work.

**A simple expression must not have to pass through a maze of unrelated modules merely to obtain a reply.** For example, a learned or unfamiliar expression such as `puurs` should be resolved by the relevant capability or receive an appropriate clarification—not trigger a serial chain of independent answer vetoes.

One governing authority boundary applies to the request. Module-level checks supply evidence and enforce their defined contracts; they do not become competing controllers or grant themselves authority.

## Diagnostics by design

Digit's diagnostic system (DSD) must be able to trace a request **from Core through Router to the specific module decision point**, including the actual source-file line number where available. Traceability is part of the architecture from the start, not an afterthought.

Core logging is mandatory: the current development runtime logs to **`./logs/digit.log`** relative to the working directory, not `audit.log`. The installed location is intended to be `/opt/digit` when permissions and deployment are ready. SA-specific checks are a later module concern, **not a prerequisite for Core startup**.

The shared diagnostic contract should make the following observable:

- Request identity and the invoked module/service.
- The routing decision and actual module execution outcome.
- Structured decision or failure code with a human-readable meaning.
- Actual source path, function, and line number **reported at the decision point**.
- Evidence used, specific recorded rejection/acceptance reason, and the final rendered outcome.
- Established authority and repair-authorization status.

**DSD reports observations; it does not guess causes, invent line numbers, repair code without authorization, or control application routing.** Core logging and diagnostic records provide a coherent request trace; each module reports its actual internal decision location and reason. DSD observes these records without becoming a routing checkpoint.

## Interface boundaries

Interfaces are **communication adapters**, not application controllers. They should receive and deliver messages, transport verified identity/context when applicable, and hand the request to Router. They should not take ownership of knowledge interpretation, Corpus logic, intent decisions, response composition, or application-wide routing.

Rebuild decisions:

- **Do not migrate OAuth or ChatGPT-specific interface integration.** Those integrations are excluded from this rebuild.
- **Leave the existing Slack integration alone**; do not casually rewrite it as part of the Interface cleanup.
- **IRC is a possible future adapter**, not an implemented or committed capability.
- Other required transports should remain narrow adapters with a common request contract rather than expand into a monolithic Interface module.

Transport-specific authentication and permissions must continue to respect the established Core authority boundaries.

## Rebuild method

1. Establish the **Core** and its common contracts, lifecycle, policy/authority boundaries, and qualification system.
2. Establish **Bootstrap** (initialize and hand off) and **Router** (route only) according to ChAoS MVC.
3. Bring over necessary **modules individually**, adapting their contracts and responsibilities to the new architecture rather than copying the old pipeline.
4. Establish **Rendering** as the view layer and keep interfaces small.
5. Verify each migrated capability against its contract, including diagnostics and negative cases, before treating it as ready.

Preserve proven behavior and useful tests without preserving unnecessary coupling. Module versioning, qualification, and authorized change control remain part of STN-LABZ development practice. Source edits and migration are subject to the project's established rules.

## Router 0.1.0 (operator accepted) and Bootstrap integration (pending acceptance)

`include/router.h` and `src/router.c` define a bounded route-to-single-service mapping. A trusted host configures routes before module start, and the Router registers `router.dispatch` through the existing ABI 1.5 service host callbacks on activation. Core authority precedes routing; an asserted `core_authorized` flag in the bounded request cannot independently authenticate its caller and is only appropriate behind an enforced Core boundary. Unknown routes, denied requests, unavailable service hosts, malformed requests, and downstream errors have explicit results. No capability modules or runtime autoactivation are introduced. Router 0.1.0 was explicitly accepted after the operator reported 21/21 suites and 1,441/1,441 assertions, zero failures. `include/bootstrap.h` and `src/bootstrap.c` now supply a separate, Core-policy-gated initialization and Router handoff lifecycle. The Bootstrap test uses the Core ABI registry, fixture authorization, Router service registration, and one mock model. This Bootstrap implementation is not automatically executed by the production zero-module Core runtime. Run `make clean && make && make test` for the original 20 Core suites, accepted Router suite and new Bootstrap suite.

## Phase 3 expression Model (pending operator acceptance)

`include/model_expression.h` and `src/model_expression.c` provide a bounded ABI 1.5 Model version 0.1.0 that registers the `expression.lookup` service. The deliberately narrow lookup fixture recognizes `purrs` with explicit evidence `fixture:expression-v1`, reports other valid expressions such as `puurs` as UNKNOWN without inventing a meaning, and distinguishes invalid inputs. The Model records its own decision source location and uses Core operational logging. No knowledge ingestion, learned Corpus, Router interpretation, external interface, Rendering, or DSD functionality is implied.

The new standalone integration test exercises **Core admission/authorization → Bootstrap → Router → one Model**, including failed/denied paths, exactly one invocation per allowed request, and stop/revocation. Production Digit remains zero-module until a later, separately authorized integration. The previously accepted **22 suites / 1,460 assertions** remain the regression baseline; the new Model suite is suite 23. Compilation and operator acceptance are still pending.

## Phase 4 Rendering View (pending operator acceptance)

`include/rendering.h` and `src/rendering.c` define Rendering View 0.1.0 under ABI 1.5. The independent `rendering.render` service receives an already-decided Router result and, for successful expression lookups only, a bounded Model result. The View formats success with supplied evidence, unknown/invalid expressions, route denial, unknown route, unavailability, and failure; it does not issue routing decisions, authorize actions, call Models, or synthesize evidence. A successful route without valid Model evidence is rejected.

The new `tests/test_rendering.c` suite demonstrates Core admission → Bootstrap → Router → Expression Model → Rendering using an isolated fixture host. It asserts exactly one Model invocation per authorized request and that rendering denial/unknown cases cannot cause extra Model invocations. Existing 23 operator-accepted suites remain baseline; **24 suites** are targeted for operator qualification. The production runtime still starts with zero activated modules; no interface or DSD has been added.

## Explicit local MVC runtime (operator qualification pending)

The production executable now links the accepted Router, Bootstrap, Expression Model and Rendering implementations. **Normal `./bin/digit` remains independent Core startup with no automatic capability activation.** To invoke the actual integrated MVC lifecycle from a local trusted development shell, run `./bin/digit --mvc`. This explicitly selected mode registers, qualifies and Core-policy-activates the Expression Model and Rendering View, then starts Router via Bootstrap. Type `purrs` or `puurs` to exercise **Core → Bootstrap → Router → Model → Rendering** inside the actual Digit process. Type `/quit` to stop and record module shutdown in `logs/digit.log`.

**Security boundary:** The local development CLI is an operator-selected test mode, *not* authenticated transport or general-purpose permission delegation. Its policy context is an in-process assertion; it does not independently establish user identity. Do not expose this mode as a remote interface. Capability modules are still built in for this explicit development integration and are not yet separately deployed dynamically loaded packages. The automated live console integration smoke test is an additional 25th suite and does not reopen previous operator-accepted test records.

## Phase 5 — DSD independent module source (pending operator verification)

`modules/dsd/` holds DSD's independent `module.conf`, sources, and Makefile. From that directory, `make` builds `bin/dsd.so` without rebuilding or changing Digit's Core. The existing ABI 1.5 loader reads `module.conf` and loads the shared object from the executable-relative `bin/modules/dsd/bin/dsd.so` after deployment. Core discovers and qualifies the module, but does not automatically activate it. An authorized Router service connection requires separately implemented configuration through normal module lifecycle, not Core modification.

The previously attempted DSD linkage into `src/mvc_console.c`, Bootstrap and Rendering has been reverted. The accepted 25-suite baseline is preserved for retest. **Do not treat DSD as a built-in Core capability.**

## Status

**Phase 1 Core foundation qualification COMPLETE AND OPERATOR ACCEPTED.** Accepted source baseline: [`bee0d22`](https://github.com/poemei/digit/commit/bee0d2297889c14d7794ed533baa72fcbc14755c). The operator reported **20/20 suites**, **1,410/1,410 assertions passed**, **0 failed assertions**, and **0 suite/report errors**. Digit independently started with zero modules, verified Agent Root revision 1, entered RUNNING, and shut down normally on SIGINT. The supplied `./logs/digit.log` session spans **2026-10-13 02:43:30–02:43:33 UTC** and records the Core lifecycle and shutdown. The operator explicitly accepted the result.

This acceptance covers the tested Phase 1 Core foundation. It does not establish external identity authentication, delivery of GO3 escalation notifications, or policy enforcement against callers invoking generic ABI functions outside Digit Core. These remain documented future integration boundaries, **not additional Phase 1 test milestones**. ABI 1.5 is unchanged. **Phase 2 Router 0.1.0 has been operator accepted. Bootstrap integration is now implemented and awaiting operator testing.** The new Router is a separate ABI 1.5 module (version 0.1.0), not a new Core controller. The Bootstrap initialization and handoff code is implemented behind trusted Core authorization; automatic production Router activation is not introduced, and the accepted Core runtime still starts with zero modules.

The governing design is simple:

> **Core provides the foundation. Bootstrap initializes. Router routes. Modules do their jobs. Rendering presents. DSD explains what happened.**
