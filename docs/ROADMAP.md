# Digit Rebuild Roadmap

**Repository:** `poemei/digit`  
**Reference source:** [`stnlabz/digit`](https://github.com/stnlabz/digit)  
**Architecture:** ChAoS MVC — **Core → Bootstrap → Router → relevant Module → Rendering**  
**Status:** Phase 1 Core foundation qualification COMPLETE AND OPERATOR ACCEPTED at revision `bee0d22`; Phase 2 Router module 0.1.0 implemented under separate operator authorization, awaiting build/test acceptance. Bootstrap and later stages are not authorized.

## 1. Non-negotiable design rules

1. **ALWAYS START WITH CORE.** Do not create, migrate, or wire a functional module until Core's foundation and acceptance gates are established.
2. **Digit's identity and core function remain in Core.** Core defines who Digit is, her foundational operating behavior, fundamental authority/policy enforcement, lifecycle, module contracts, registration, qualification, and controlled execution infrastructure. Do not move those responsibilities into modules.
3. **Modules have nothing to do with implementing Core.** Modules are external capabilities managed by Core. They must not define Digit's identity, rewrite Core rules, own Core lifecycle, or become prerequisites for Core to exist, start, or operate at its foundational level.
4. **Bootstrap initializes and hands off. Router routes only.** Router maps `/ask` to Ask, `/diagnose` to DSD, etc., through established routes. It does not interpret questions, compose answers, validate responses, or orchestrate a chain of modules. Modules own their work and return results for Rendering.
5. **Rendering is the View.** Models produce results, Router selects their route, and Rendering presents the result. An Interface is a transport adapter, not the View or Controller.
6. **One governing authority boundary.** Core enforces the foundational rules and authority model. No module can override them, grant itself authority, or silently redirect command flow.
7. **Evidence over guesses.** DSD must report actual observed decisions, including source file/function/line when supplied by the responsible code. No fabricated locations, root causes, or repair claims.
8. **Keep it simple.** A casual learned expression such as `purrs` must not require sequential approval from unrelated modules before Digit can answer or ask for clarification.
9. **Migrate features, not the old architecture.** The previous repository is reference material; nothing is copied simply because it previously existed.
10. **Core logging is mandatory.** Use Core-owned `./logs/digit.log` (working-directory relative) during development; do not create `audit.log`. SA functionality is a future optional module, not Core.
11. **Qualification is mandatory.** Each stage must meet its acceptance criteria before the next stage begins. Preserve STN-LABZ module versioning and authorization rules for subsequent source changes.

## 2. Boundaries and dependency direction

```text
                     DIGIT CORE
        identity | core function | general orders
       policy/authority | lifecycle | qualification
          module registry | stable service ABI
                         |
                         v
                    DISPATCHER
                single MVC Controller
            request routing + trace ownership
               /          |           \
              v           v            v
         MODEL A       MODEL B       MODEL N
       (capability)  (capability)   (capability)
              \           |           /
               \          v          /
                  RENDERING (View)
                         |
                  TRANSPORT ADAPTER
             console / existing Slack / IRC?
```

Diagram shows responsibilities, **not** a mandatory chain of capability modules. Router selects the one relevant route/module (e.g. `/ask` -> Ask); a module returns its result for rendering. Rendering and transport deliver the result. Core does not depend on implementation details of any module.

**Allowed dependency:** module → stable Core-provided ABI/contracts.  
**Not allowed:** Core → concrete model implementation, model → control of Core, or model → arbitrary next model for routing.  
**Router:** single routing component within Core's established boundaries; routing must not become interpretation, composition, or business-logic coordination.

## 3. Build stages and hard gates

**All phase exit gates also require the [operator acceptance and stop rules](../AGENTS.md).** Automated qualification and module activation are engineering evidence only. Stop for the operator-selected end-to-end demonstration, explicit acceptance, and authorization of the next named step. For current Core foundation development, the operator explicitly selects build, aggregate test report, Core console run, and local `digit.log` evidence as the acceptance demonstration. These are evidence for the operator's decision, never automatic acceptance. APP/Slack end-to-end checks apply once those interfaces exist.

### Phase 0 — Baseline and reference inventory (documentation only)

- [ ] Freeze and identify the old repository revision used for reference.
- [ ] Inventory existing Core facilities, Router responsibilities, interfaces, module APIs, qualification tests, DSD, and Rendering.
- [ ] Classify each old feature as **Core foundation**, **Router/controller**, **Model**, **View**, **transport**, **defer**, or **exclude**.
- [ ] Record coupling/architectural defects; do not import them into the new project.
- [ ] Establish build/toolchain conventions, source layout, and a minimal change/version policy.

**Exit gate:** an approved responsibility inventory and Core scope. No new modules are implemented in this phase.

### Phase 1 — CORE FIRST: identity and foundational behavior

Implement only the independent Core.

- [ ] Define Digit's permanent program identity and foundational core function in Core.
- [ ] Implement the three established General Orders at Core level:
  1. Remain at your assigned mission.
  2. Follow established policies and authorized instructions.
  3. Report anything outside your authority to the appropriate next level of authority.
- [ ] Establish policy and authority enforcement with clear denial and escalation behavior; modules cannot override the foundation.
- [ ] Create Core initialization, deterministic startup/shutdown, health/failure reporting, and basic operator-facing core status without depending on any module.
- [ ] Define the minimal stable module ABI: descriptor/identity, lifecycle callbacks, service contract, status and error result types.
- [ ] Define registration, discovery, lifecycle states, module isolation boundaries, and explicit failure outcomes.
- [ ] Build qualification/admission checks in Core. Missing, incompatible, or unqualified modules must not compromise Core startup or identity.
- [ ] Define trace metadata contract (request ID, decision code, function/file/line, reason) for later module use without requiring DSD itself.
- [ ] Add Core unit and negative tests for identity, foundational rules, lifecycle failure, version/ABI mismatch, denial, and no-modules startup.

**Hard acceptance gate — Core stands alone:**
- Core builds and starts with **zero external modules**.
- Core retains identity and foundational functions with every module absent or failing.
- Core consistently enforces its policy/authority boundaries.
- Core rejects or isolates unqualified/incompatible modules as specified, with actual recorded reasons.
- Core qualification and negative tests pass. **Do not begin Router until this Core gate has explicit operator acceptance and the operator separately authorizes the Router step.**

### Phase 2 — Bootstrap and Router: initialization and single-route dispatch

**Current implementation status:** Router 0.1.0 **operator accepted**, with 21/21 suites and 1,441/1,441 assertions passing. Bootstrap initialization and handoff harness now implemented, **awaiting operator verification and acceptance**. Router is configured explicitly before start and dispatches a single registered service through the ABI host; it never selects a chain of models. Bootstrap now admits, verifies, qualifies, Core-authorizes and activates Router in its explicit test harness, then hands off exactly one request. The production Core runtime still autoactivates no modules; no external interface or application capability is implemented. The previously accepted 20 Core suites remain baseline; the Router suite is additional. No Phase 2 exit is claimed before the test harness establishes an accepted Core/Router/mock-module execution path.

After operator acceptance of Core and explicit authorization, implement minimal Bootstrap initialization/handoff and Router's direct route mapping.

- [ ] Register Router through the Core module ABI; no special-purpose Core implementation of Router logic.
- [ ] Define one bounded request/result contract with verified actor/context, request ID, routing outcome, and structured trace.
- [ ] Separate *whether a request is permitted* (Core authority) from *which capability handles it* (Router routing).
- [ ] Keep Router restricted to mapping one request to its owning module; the module owns execution and returns the result.
- [ ] Enforce no automatic module-to-module orchestration and no duplicate Controllers in Interfaces or Models.
- [ ] Provide explicit outcomes for unsupported, unknown, ambiguous, denied, and failed requests without guessing or silently executing.
- [ ] Include tests for deterministic routing, absent services, denied execution, and controller trace integrity.

**Exit gate:** through a minimal test harness, Core admits Router; Router routes a synthetic request to exactly one mock module and returns a traceable result. No chain of independent processing gates.

### Phase 3 — Model contracts and minimal capability

**Implementation submitted for operator qualification (failure repair in progress):** Expression Model 0.1.0 defines an ABI 1.5 `expression.lookup` contract, request/result status, explicit fixture evidence and responsible source decision line. A dedicated test harness runs the complete Core-policy-admitted Model with Bootstrap and Router, and checks known `purrs`, unknown `puurs`, invalid input, denial, and clean lifecycle. This is a small static test fixture, not a learned vocabulary or general language understanding. No automatic production Model activation was introduced. Accepted baseline: 22 suites, 1,460 assertions. Current target: 23 suites, subject to operator build/testing and explicit acceptance.


- [ ] Define the narrow Model interface: inputs, outputs, errors, provenance, actual decision location, and qualification tests.
- [ ] Select **one** minimum viable capability (e.g. recognized/unknown expression lookup) and implement it independently.
- [ ] Ensure the Model never changes Digit's identity, Core policy, routing ownership, or the next module.
- [ ] Define optional interpretation or context assistance as capabilities invoked only when required—not automatic checkpoints.
- [ ] Preserve factual uncertainty: no invented meanings or unearned conversational answers.
- [ ] Test recognized, unknown, ambiguous, and unavailable-service cases.

**Exit gate:** a simple request (such as `purrs` when a learned relation exists) completes **Core → Router → relevant Model → result**, without invoking unrelated Models. Unknown `puurs` returns a bounded unknown/clarification outcome.

### Phase 4 — Rendering: the View

**Implementation under operator qualification:** Rendering View 0.1.0 supplies a presentation-only ABI 1.5 service, with bounded immutable-copy input combining Router decision and optional Model result. The added 24th suite demonstrates the complete MVC fixture execution path through the standalone View and guards against View invocation of Models or invented evidence. This phase has **not** been accepted or activated in the production runtime; the 23-suite, 1,486-assertion accepted baseline remains unchanged pending the operator's test evidence.


- [ ] Define a presentation-neutral result contract emitted by Router.
- [ ] Implement a Rendering module that formats successes, clarification prompts, denials, and diagnostics without deciding authority or routing.
- [ ] Add tests proving Rendering cannot modify the Controller's decision or fabricate evidence.

**Exit gate:** end-to-end demonstration **Core → Router → Model → Rendering** with exactly the modules needed.

### Post-Phase 4 — Explicit local MVC process integration (pending acceptance)

The operator authorized integration of the accepted MVC components into the running executable. The `--mvc` local development console starts Core-qualified, explicitly activated Model/View/Router components through the common ABI contract and performs end-to-end local request/response with mandatory logs. Default `./bin/digit` remains unchanged. This provides real process-level MVC evidence, not merely isolated module harness tests, but **it is not an authenticated general-purpose interface** and modules remain linked into the executable for the development trial. The new process-level smoke test is suite 25; operator testing and explicit acceptance are required before treating this integration as accepted.

### Phase 5 — DSD: observe the actual path

**Authorized implementation awaiting operator acceptance:** DSD 0.1.0 is an independent ABI 1.5 service that reports bounded request-correlated observations, actual source locations when supplied, and explicit missing-evidence fields. The local MVC console hands `/diagnose` to Router, which dispatches to the regular `dsd.report` Model service, with Rendering presenting results without replaying Model execution; a new independent DSD suite covers Core qualification and authorization, failure reporting, missing and malformed observations, and shutdown. The accepted baseline is 25 suites / 1,532 assertions; the target is 26 suites, with no claimed pass until operator verification. DSD remains an independently controlled Router-selected capability and is not inserted into the normal expression route. Deeper historical trace collection and separately dynamically loaded deployment remain unimplemented.


- [ ] Implement DSD as a diagnostic capability, not an execution controller.
- [ ] Have Router associate structured module observations with a single request ID.
- [ ] Require the code making a decision to report its own actual source path, function, line, decision code, and reason.
- [ ] Preserve request-local trace across an authorized diagnostic follow-up without replaying the original action.
- [ ] Show accepted/rejected steps, missing evidence, and the exact responsible source location where available.
- [ ] Mark unavailable information explicitly **UNKNOWN**; never infer line numbers or unobserved root causes.
- [ ] Keep repair authorization explicit; diagnosis alone grants no repair permission.

**Exit gate:** a forced mock failure produces a DSD report pointing to the actual recorded failure site and reason; no separate tracing maze is required.

### Phase 6 — Thin transport interfaces

- [ ] Establish a single transport-to-Router request contract and transport-agnostic result delivery.
- [ ] Keep adapters limited to receiving/sending data and supplying independently verified transport identity/context.
- [ ] Provide a minimal console path for acceptance testing.
- [ ] Keep existing Slack integration intact unless the operator separately authorizes a change.
- [ ] Keep IRC as an optional future adapter, not an implied current dependency.
- [ ] **Exclude OAuth and ChatGPT-specific Interface integration** from this rebuild.
- [ ] Prevent any adapter from owning Corpus processing, classification, composition, or routing policy.

**Exit gate:** changing transports does not change business routing or require edits to Core's foundational identity and functions.

### Phase 7 — Controlled feature migration

For each candidate capability from `stnlabz/digit`:

1. State the capability and its owner: Core / Router / Model / Rendering / transport.
2. Confirm that migration does not violate the Core boundary or create duplicate Controllers.
3. Define its ABI and acceptance tests before migrating code.
4. Migrate the smallest useful behavior; remove old orchestration and redundant validation chains.
5. Qualify, validate versioning, test in isolation, and then test controller integration.
6. Record source provenance, changed files, known limitations, and actual verification evidence.

**Exit gate:** functionality is admitted independently with no new mandatory routing chain.

## 4. Required architectural acceptance scenarios

| Scenario | Required result |
| --- | --- |
| Zero modules installed | Core still starts, identifies Digit, enforces core function and policies |
| Unqualified module | Core rejects admission or activation, without losing foundational operation |
| `purrs` with recognized authorized lesson | Router invokes relevant capability and obtains a bounded conversational or clarification result |
| Unknown `puurs` | Clear unknown/clarification result without fabricated knowledge |
| Capability unavailable | Router reports specific failure without routing through unrelated modules |
| Unauthorized command | Core authority controls deny; no module may elevate privileges |
| Diagnostic request | DSD follows retained request trace, including actual source location if observed |
| New transport | Adapter translates to the common request contract; no duplicated controller logic |
| Rendering failure | Decision/evidence remain intact; the View cannot silently override outcomes |

## 5. Development discipline

- **Do not work ahead of the current gate.** Build Core first; expand outward only after the current step's explicit operator acceptance and authorization of the next named step. Qualification or activation alone is insufficient.
- Keep Core minimal, independent, and stable. **Adding a feature is not a reason to enlarge Core.**
- Record every module change with its required version increment, and update relevant tests/documentation.
- Use actual compiler/test/runtime output for acceptance; committed source is not proof of successful compilation or hotload.
- Maintain explicit human authorization for changes, admission, and repair.
- The old repository remains reference material; this repository is the authoritative rebuild.

## 6. Current milestone

**CURRENT: Phase 1 Core foundation qualification COMPLETE AND OPERATOR ACCEPTED.** The operator's accepted baseline is revision [`bee0d22`](https://github.com/poemei/digit/commit/bee0d2297889c14d7794ed533baa72fcbc14755c), **20 of 20 suites**, **1,410 of 1,410 assertions**, **0 failures**, and **0 suite/report errors**. Core started and stopped normally with zero modules. The operator's recorded session on 2026-10-13 ran from 02:43:30 to 02:43:33 UTC, with mandatory `logs/digit.log` evidence and explicit acceptance. The checklist above remains the original planned requirements; the controlling qualification outcome is this operator-accepted evidence. Core owns essential operational audit; detailed diagnostic trace/correlation belongs to a future DSD module. Generic ABI bypass, caller authentication and actual escalation notification delivery are tracked as future integration limits, not new Core qualification suites.  
No Bootstrap, Router, Models, Views, or transport integration work is authorized by this roadmap alone. Work proceeds only under operator direction and after the applicable gate is met.


**Phase 3 qualification repair:** Operator run at 23 suites / 1,485 assertions exposed that Bootstrap shutdown used the Core stop-all operation, thereby stopping an independent Model along with Router. The corrected Bootstrap lifecycle now requests an audited, Core-owned stop of **only Router**; the Model remains ACTIVE until Core explicitly stops it. The test asserts this isolation. This change is limited to the observed failure and must be retested and accepted by the operator. No Phase 3 acceptance is claimed.
