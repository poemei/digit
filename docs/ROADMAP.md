# Digit Rebuild Roadmap

**Repository:** `poemei/digit`  
**Reference source:** [`stnlabz/digit`](https://github.com/stnlabz/digit)  
**Architecture:** ChAoS MVC — **Core → Dispatcher → Modules**  
**Status:** Planning; implementation has not begun.

## 1. Non-negotiable design rules

1. **ALWAYS START WITH CORE.** Do not create, migrate, or wire a functional module until Core's foundation and acceptance gates are established.
2. **Digit's identity and core function remain in Core.** Core defines who Digit is, her foundational operating behavior, fundamental authority/policy enforcement, lifecycle, module contracts, registration, qualification, and controlled execution infrastructure. Do not move those responsibilities into modules.
3. **Modules have nothing to do with implementing Core.** Modules are external capabilities managed by Core. They must not define Digit's identity, rewrite Core rules, own Core lifecycle, or become prerequisites for Core to exist, start, or operate at its foundational level.
4. **Dispatcher is the one application Controller/router**, attached through Core's module mechanism. It selects and coordinates work. Other modules return results to it; they do not form a mandatory daisy-chain of controllers.
5. **Rendering is the View.** Models produce results, Dispatcher determines routing/outcome, and Rendering presents the result. An Interface is a transport adapter, not the View or Controller.
6. **One governing authority boundary.** Core enforces the foundational rules and authority model. No module can override them, grant itself authority, or silently redirect command flow.
7. **Evidence over guesses.** DSD must report actual observed decisions, including source file/function/line when supplied by the responsible code. No fabricated locations, root causes, or repair claims.
8. **Keep it simple.** A casual learned expression such as `purrs` must not require sequential approval from unrelated modules before Digit can answer or ask for clarification.
9. **Migrate features, not the old architecture.** The previous repository is reference material; nothing is copied simply because it previously existed.
10. **Qualification is mandatory.** Each stage must meet its acceptance criteria before the next stage begins. Preserve STN-LABZ module versioning and authorization rules for subsequent source changes.

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

Diagram shows responsibilities, **not** a mandatory execution chain. Dispatcher invokes only relevant capability modules; a module returns its result to the controller. Rendering and transport deliver the result. Core does not depend on implementation details of any module.

**Allowed dependency:** module → stable Core-provided ABI/contracts.  
**Not allowed:** Core → concrete model implementation, model → control of Core, or model → arbitrary next model for routing.  
**Dispatcher:** a module serving the Controller role; its selection and routing authority exists only within the boundaries established by Core.

## 3. Build stages and hard gates

### Phase 0 — Baseline and reference inventory (documentation only)

- [ ] Freeze and identify the old repository revision used for reference.
- [ ] Inventory existing Core facilities, Dispatcher responsibilities, interfaces, module APIs, qualification tests, DSD, and Rendering.
- [ ] Classify each old feature as **Core foundation**, **Dispatcher/controller**, **Model**, **View**, **transport**, **defer**, or **exclude**.
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
- Core qualification and negative tests pass. **Do not begin Dispatcher until this gate is green.**

### Phase 2 — Dispatcher: one Controller / one route owner

Implement Dispatcher as the first application module, **after** the Core gate.

- [ ] Register Dispatcher through the Core module ABI; no special-purpose Core implementation of Dispatcher logic.
- [ ] Define one bounded request/result contract with verified actor/context, request ID, routing outcome, and structured trace.
- [ ] Separate *whether a request is permitted* (Core authority) from *which capability handles it* (Dispatcher routing).
- [ ] Make Dispatcher the sole coordinator: select service(s) needed for the request, invoke, collect results, decide final disposition.
- [ ] Enforce no automatic module-to-module orchestration and no duplicate Controllers in Interfaces or Models.
- [ ] Provide explicit outcomes for unsupported, unknown, ambiguous, denied, and failed requests without guessing or silently executing.
- [ ] Include tests for deterministic routing, absent services, denied execution, and controller trace integrity.

**Exit gate:** through a minimal test harness, Core admits Dispatcher; Dispatcher routes a synthetic request to exactly one mock module and returns a traceable result. No chain of independent processing gates.

### Phase 3 — Model contracts and minimal capability

- [ ] Define the narrow Model interface: inputs, outputs, errors, provenance, actual decision location, and qualification tests.
- [ ] Select **one** minimum viable capability (e.g. recognized/unknown expression lookup) and implement it independently.
- [ ] Ensure the Model never changes Digit's identity, Core policy, routing ownership, or the next module.
- [ ] Define optional interpretation or context assistance as capabilities invoked only when required—not automatic checkpoints.
- [ ] Preserve factual uncertainty: no invented meanings or unearned conversational answers.
- [ ] Test recognized, unknown, ambiguous, and unavailable-service cases.

**Exit gate:** a simple request (such as `purrs` when a learned relation exists) completes **Core → Dispatcher → relevant Model → result**, without invoking unrelated Models. Unknown `puurs` returns a bounded unknown/clarification outcome.

### Phase 4 — Rendering: the View

- [ ] Define a presentation-neutral result contract emitted by Dispatcher.
- [ ] Implement a Rendering module that formats successes, clarification prompts, denials, and diagnostics without deciding authority or routing.
- [ ] Add tests proving Rendering cannot modify the Controller's decision or fabricate evidence.

**Exit gate:** end-to-end demonstration **Core → Dispatcher → Model → Rendering** with exactly the modules needed.

### Phase 5 — DSD: observe the actual path

- [ ] Implement DSD as a diagnostic capability, not an execution controller.
- [ ] Have Dispatcher associate structured module observations with a single request ID.
- [ ] Require the code making a decision to report its own actual source path, function, line, decision code, and reason.
- [ ] Preserve request-local trace across an authorized diagnostic follow-up without replaying the original action.
- [ ] Show accepted/rejected steps, missing evidence, and the exact responsible source location where available.
- [ ] Mark unavailable information explicitly **UNKNOWN**; never infer line numbers or unobserved root causes.
- [ ] Keep repair authorization explicit; diagnosis alone grants no repair permission.

**Exit gate:** a forced mock failure produces a DSD report pointing to the actual recorded failure site and reason; no separate tracing maze is required.

### Phase 6 — Thin transport interfaces

- [ ] Establish a single transport-to-Dispatcher request contract and transport-agnostic result delivery.
- [ ] Keep adapters limited to receiving/sending data and supplying independently verified transport identity/context.
- [ ] Provide a minimal console path for acceptance testing.
- [ ] Keep existing Slack integration intact unless the operator separately authorizes a change.
- [ ] Keep IRC as an optional future adapter, not an implied current dependency.
- [ ] **Exclude OAuth and ChatGPT-specific Interface integration** from this rebuild.
- [ ] Prevent any adapter from owning Corpus processing, classification, composition, or routing policy.

**Exit gate:** changing transports does not change business routing or require edits to Core's foundational identity and functions.

### Phase 7 — Controlled feature migration

For each candidate capability from `stnlabz/digit`:

1. State the capability and its owner: Core / Dispatcher / Model / Rendering / transport.
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
| `purrs` with recognized authorized lesson | Dispatcher invokes relevant capability and obtains a bounded conversational or clarification result |
| Unknown `puurs` | Clear unknown/clarification result without fabricated knowledge |
| Capability unavailable | Dispatcher reports specific failure without routing through unrelated modules |
| Unauthorized command | Core authority controls deny; no module may elevate privileges |
| Diagnostic request | DSD follows retained request trace, including actual source location if observed |
| New transport | Adapter translates to the common request contract; no duplicated controller logic |
| Rendering failure | Decision/evidence remain intact; the View cannot silently override outcomes |

## 5. Development discipline

- **Do not work ahead of the current gate.** Build Core first; expand outward only after demonstrated qualification.
- Keep Core minimal, independent, and stable. **Adding a feature is not a reason to enlarge Core.**
- Record every module change with its required version increment, and update relevant tests/documentation.
- Use actual compiler/test/runtime output for acceptance; committed source is not proof of successful compilation or hotload.
- Maintain explicit human authorization for changes, admission, and repair.
- The old repository remains reference material; this repository is the authoritative rebuild.

## 6. Current milestone

**NEXT: Phase 0 documentation/inventory, followed by Phase 1 Core.**  
No Dispatcher, Models, Views, transport integration, or migration work is authorized by this roadmap alone. Work proceeds only under operator direction and after the applicable gate is met.
