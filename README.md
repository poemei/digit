# Digit — Architectural Rebuild

Digit is an STN-LABZ modular autonomous-agent project. This repository is a **clean architectural rebuild** of Digit, using the established ChAoS MVC design as its structural reference.

The existing [stnlabz/digit](https://github.com/stnlabz/digit) repository is the **reference implementation**: it provides source material and previously developed functionality, not an architecture to copy wholesale. Working features may be migrated only after being realigned with the design and qualification requirements here.

## Mandatory development stop

**Module activation is not operator acceptance.** Build/test success, qualification GREEN and HOTLOAD_ACTIVE do not authorize moving to the next step. Follow [the operator acceptance gate in AGENTS.md](AGENTS.md): hand over the exact client test procedure, stop for the operator's APP/Slack/other selected end-to-end testing, and wait for explicit acceptance plus authorization of the next named step. This applies to every rebuild phase.

## Governing architecture

**Core → Dispatcher → Modules**

- **Core** is the foundation. It owns module lifecycle, registration, service contracts, qualification gates, and the infrastructure that enforces established policies and authority boundaries. Build and verify Core first; expand outward.
- **Dispatcher** is the application's **single controller/router**. It receives requests via the applicable interface, selects appropriate module services, coordinates the request, and records an end-to-end execution trace. It does not delegate control of the overall request flow to a chain of modules.
- **Modules** perform their own bounded jobs and return structured results to Dispatcher. Knowledge, language interpretation, diagnostics, validation, and other capabilities belong in modules according to their contracts—not as mandatory successive routing checkpoints for every request.
- **Rendering** provides the **view**: it presents results and diagnostic reports. Presentation must not become another source of business logic or routing authority.

This is **MVC in the ChAoS MVC pattern**: Dispatcher is the Controller; capability modules are Models; Rendering supplies the Views. Core provides the common foundation.

### Request flow

1. An interface receives a request and passes along verified transport/session context.
2. Core's established authority and policy controls remain applicable; Dispatcher coordinates the authorized request through the relevant module or modules.
3. Each invoked module does its assigned work and returns a bounded result, including evidence of its decisions or failures.
4. Dispatcher determines the outcome and hands the result to Rendering for presentation.

**A simple expression must not have to pass through a maze of unrelated modules merely to obtain a reply.** For example, a learned or unfamiliar expression such as `puurs` should be resolved by the relevant capability or receive an appropriate clarification—not trigger a serial chain of independent answer vetoes.

One governing authority boundary applies to the request. Module-level checks supply evidence and enforce their defined contracts; they do not become competing controllers or grant themselves authority.

## Diagnostics by design

Digit's diagnostic system (DSD) must be able to trace a request **from Core through Dispatcher to the specific module decision point**, including the actual source-file line number where available. Traceability is part of the architecture from the start, not an afterthought.

The shared diagnostic contract should make the following observable:

- Request identity and the invoked module/service.
- The routing decision and actual module execution outcome.
- Structured decision or failure code with a human-readable meaning.
- Actual source path, function, and line number **reported at the decision point**.
- Evidence used, specific recorded rejection/acceptance reason, and the final rendered outcome.
- Established authority and repair-authorization status.

**DSD reports observations; it does not guess causes, invent line numbers, repair code without authorization, or control application routing.** Dispatcher maintains the coherent request trace; each module is responsible for reporting its own internal decision location and reason.

## Interface boundaries

Interfaces are **communication adapters**, not application controllers. They should receive and deliver messages, transport verified identity/context when applicable, and hand the request to Dispatcher. They should not take ownership of knowledge interpretation, Corpus logic, intent decisions, response composition, or application-wide routing.

Rebuild decisions:

- **Do not migrate OAuth or ChatGPT-specific interface integration.** Those integrations are excluded from this rebuild.
- **Leave the existing Slack integration alone**; do not casually rewrite it as part of the Interface cleanup.
- **IRC is a possible future adapter**, not an implemented or committed capability.
- Other required transports should remain narrow adapters with a common request contract rather than expand into a monolithic Interface module.

Transport-specific authentication and permissions must continue to respect the established Core authority boundaries.

## Rebuild method

1. Establish the **Core** and its common contracts, lifecycle, policy/authority boundaries, and qualification system.
2. Establish **Dispatcher** as the sole application request router and controller, with request-scoped trace collection.
3. Bring over necessary **modules individually**, adapting their contracts and responsibilities to the new architecture rather than copying the old pipeline.
4. Establish **Rendering** as the view layer and keep interfaces small.
5. Verify each migrated capability against its contract, including diagnostics and negative cases, before treating it as ready.

Preserve proven behavior and useful tests without preserving unnecessary coupling. Module versioning, qualification, and authorized change control remain part of STN-LABZ development practice. Source edits and migration are subject to the project's established rules.

## Status

**Architectural rebuild / foundation stage.** This document records the intended development structure; it does not claim that the new Core, Dispatcher, modules, interfaces, or runtime have already been implemented.

The governing design is simple:

> **Core provides the foundation. Dispatcher routes. Modules do their jobs. Rendering presents. DSD explains what happened.**
