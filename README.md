# Digit — Architectural Rebuild

Digit is an STN-LABZ modular autonomous-agent project. This repository is a **clean architectural rebuild** of Digit, using the established ChAoS MVC design as its structural reference.

The existing [stnlabz/digit](https://github.com/stnlabz/digit) repository is the **reference implementation**: it provides source material and previously developed functionality, not an architecture to copy wholesale. Working features may be migrated only after being realigned with the design and qualification requirements here.

## Mandatory development stop

**Module activation is not operator acceptance.** Build/test success, qualification GREEN and HOTLOAD_ACTIVE do not authorize moving to the next step. Follow [the operator acceptance gate in AGENTS.md](AGENTS.md): hand over the exact operator-selected test procedure, stop for the operator's Core build/test/console/log evaluation (APP/Slack testing once available), and wait for explicit acceptance plus authorization of the next named step. This applies to every rebuild phase.

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

## Status

**Phase 1 Core foundation qualification COMPLETE AND OPERATOR ACCEPTED.** Accepted source baseline: [`bee0d22`](https://github.com/poemei/digit/commit/bee0d2297889c14d7794ed533baa72fcbc14755c). The operator reported **20/20 suites**, **1,410/1,410 assertions passed**, **0 failed assertions**, and **0 suite/report errors**. Digit independently started with zero modules, verified Agent Root revision 1, entered RUNNING, and shut down normally on SIGINT. The supplied `./logs/digit.log` session spans **2026-10-13 02:43:30–02:43:33 UTC** and records the Core lifecycle and shutdown. The operator explicitly accepted the result.

This acceptance covers the tested Phase 1 Core foundation. It does not establish external identity authentication, delivery of GO3 escalation notifications, or policy enforcement against callers invoking generic ABI functions outside Digit Core. These remain documented future integration boundaries, **not additional Phase 1 test milestones**. ABI 1.5 is unchanged. **Phase 2 Bootstrap and Router have not begun and require separate operator authorization.**

The governing design is simple:

> **Core provides the foundation. Bootstrap initializes. Router routes. Modules do their jobs. Rendering presents. DSD explains what happened.**
