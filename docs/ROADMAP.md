# Digit Rebuild Roadmap

**Repository:** `poemei/digit`  
**Reference source:** [`stnlabz/digit`](https://github.com/stnlabz/digit)  
**Current operator-directed operational architecture:** **Digit → Router → Module**; no Bootstrap in the production console path  
**Status:** See the 2026-10-13 accepted operational record below. Historical phase record: Phase 1 Core foundation qualification COMPLETE AND OPERATOR ACCEPTED at revision `bee0d22`; Phase 2 Router module 0.1.0 implemented under separate operator authorization, awaiting build/test acceptance. Bootstrap and later stages are not authorized.

## Agent-level release version — OPERATOR ACCEPTED (2026-10-13)

**Digit 1.0.0** is the operator-designated stable agent version (no RC). Implementation adds a Core-owned version constant and read-only version service, native CLI display, and Ask 0.1.6 public version queries for IRC and Windows GUI. Module versions remain separate. The operator observed `version?` → `Digit 1.0.0` in the native console and `STN_Boss: digit-bot: version?` → `digit-bot: Digit 1.0.0` over IRC, and explicitly accepted Digit 1.0.0 after the IRC test. The previously reported aggregate Core test run had 26 suites, 1,548 assertions, one failure due to the outdated console banner expectation; that expectation was patched in `tests/test_mvc_console.sh`. A subsequent clean aggregate test result was not furnished in this conversation. Windows GUI version reporting remains without a separate live-test transcript.

## Current verified operational record — 2026-10-13

The phase narrative below retains historical qualification snapshots and is **not** the current installed-capability inventory. The following explicitly accepted milestones supersede older "future IRC" and "no capability implemented" descriptions where they conflict. A documented accepted scenario does not imply every planned phase exit gate is accepted.

| Capability | Source version | Operator evidence and decision |
| --- | --- | --- |
| IRC per-sender conversation and DSD report framing | IRC 0.1.1 | **ACCEPTED** after STN_Boss/PoeMei scoped diagnostic transcripts, with multiline IRC reports |
| Ask factual question to read-only Corpus | Ask 0.1.4 + Corpus 0.1.2 | **ACCEPTED** after a Corpus-backed organizational answer with record and source |
| Interpretation service and Ask integration | Interpretation 0.1.0 + Ask 0.1.5 | **ACCEPTED** after varied capability and knowledge questions through IRC |
| Native-console Corpus learning | Corpus 0.1.3 | **OPERATOR ACCEPTED — 2026-10-13** |

**Demonstrated:** `what can you tell me?` and `what are your capabilities?` produce an accurate bounded capability description; `tell me about the Chaos Foundation`, `describe Sentinel`, and `describe the Chaos MVC` resolve through Corpus with record IDs and source attribution. This establishes useful supported phrasing families—not unrestricted semantic interpretation, unknown-intent coverage, factual validation of stored claims, or independent operator authentication.

**Architecture and authority:** Digit Core → Router → Ask, with Ask calling read-only `interpretation.classify` and `corpus.lookup` services when needed. Router does not interpret, learn, or make capability decisions. IRC and Windows remain transport clients, not command-authority sources. `learn` is a native console command and no public write service is registered. Current local-console identity metadata is not a cryptographic gate.

**Operator acceptance recorded:** Corpus 0.1.3 CLI `learn` was explicitly accepted on 2026-10-13. Build logs, deployed append/readback transcripts, and negative interface test results were not supplied in this conversation, so those details are not independently documented. Interpretation unknown/ambiguous-intent behavior and adversarial language inputs were not included in the accepted IRC transcripts. No unrequested refactor or phase progression is authorized by these notes.

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

**Runtime integration repair awaiting operator acceptance:** The `--mvc` adapter discovers independently deployed DSD through existing `module.conf` discovery, verifies/qualifies/activates through existing Core gates, and configures Router to dispatch `/diagnose` to `dsd.report`. Rendering presents the returned report; no direct console→DSD calls. A new suite runs the executable against a separately built shared library in an isolated installation. The accepted 25-suite baseline is preserved; actual 26-suite results and installed runtime evidence are required. Neither Core nor ABI implementation was edited for this fix.



**Corrected module boundary, pending operator verification:** DSD source, `module.conf`, and independent build now reside in `modules/dsd/`. The existing Core dynamic discovery scans deployed module directories next to the executable, reads the manifest, and loads the ABI 1.5 shared object. The DSD-specific Bootstrap, Rendering, and local console integration was reverted. The module remains unactivated until an authorized Router and host-service configuration makes `/diagnose` available; no Core modification is justified by adding this capability. The accepted 25-suite baseline must be retested.

- [ ] Implement DSD as a diagnostic capability, not an execution controller.
- [ ] Have Router associate structured module observations with a single request ID.
- [ ] Require the code making a decision to report its own actual source path, function, line, decision code, and reason.
- [ ] Preserve request-local trace across an authorized diagnostic follow-up without replaying the original action.
- [ ] Show accepted/rejected steps, missing evidence, and the exact responsible source location where available.
- [ ] Mark unavailable information explicitly **UNKNOWN**; never infer line numbers or unobserved root causes.
- [ ] Keep repair authorization explicit; diagnosis alone grants no repair permission.

**Exit gate:** a forced mock failure produces a DSD report pointing to the actual recorded failure site and reason; no separate tracing maze is required.

### Phase 6 — Thin transport interfaces

**Operational update (2026-10-13):** IRC 0.1.1 was demonstrated and explicitly accepted for scoped diagnostics. The historical checklist below remains the original planning record; its "optional future adapter" wording is no longer an accurate current-state claim. Windows GUI existed for earlier Ask use, but this acceptance record is based on IRC demonstrations.

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

## 6. Historical Core milestone

**CURRENT: Phase 1 Core foundation qualification COMPLETE AND OPERATOR ACCEPTED.** The operator's accepted baseline is revision [`bee0d22`](https://github.com/poemei/digit/commit/bee0d2297889c14d7794ed533baa72fcbc14755c), **20 of 20 suites**, **1,410 of 1,410 assertions**, **0 failures**, and **0 suite/report errors**. Core started and stopped normally with zero modules. The operator's recorded session on 2026-10-13 ran from 02:43:30 to 02:43:33 UTC, with mandatory `logs/digit.log` evidence and explicit acceptance. The checklist above remains the original planned requirements; the controlling qualification outcome is this operator-accepted evidence. Core owns essential operational audit; detailed diagnostic trace/correlation belongs to a future DSD module. Generic ABI bypass, caller authentication and actual escalation notification delivery are tracked as future integration limits, not new Core qualification suites.  
No Bootstrap, Router, Models, Views, or transport integration work is authorized by this roadmap alone. Work proceeds only under operator direction and after the applicable gate is met.


**Phase 3 qualification repair:** Operator run at 23 suites / 1,485 assertions exposed that Bootstrap shutdown used the Core stop-all operation, thereby stopping an independent Model along with Router. The corrected Bootstrap lifecycle now requests an audited, Core-owned stop of **only Router**; the Model remains ACTIVE until Core explicitly stops it. The test asserts this isolation. This change is limited to the observed failure and must be retested and accepted by the operator. No Phase 3 acceptance is claimed.

## 2026-10-16/17 — Operator-supplied conversational regression checkpoint (IRC; NOT ACCEPTED)

**Scope:** `poemei/digit` only. This is a continuity/evidence record, not permission for further implementation, hotload, deployment, or a new phase. Core, Router and ABI are outside the authorized repair scope and were not changed in this sequence. Follow `AGENTS.md` acceptance gates; source commits, unit tests, qualification, hotload, client results and explicit operator acceptance are distinct.

### Objective and failure chronology

The operator is pursuing natural, context-aware participation in both IRC and Slack **before** expanding Digit's engineering functions. Digit briefly contributed to conversation; subsequent attempts to stop raw Corpus-data output were followed by inconsistent participation, repetitive Corpus-derived replies and generic responses to ordinary speech. The requirement is **not** to make Digit speak at every mention: she should interpret conversational context and contribute when genuinely supported, otherwise remain silent. Corpus represents learned knowledge, not a canned reply system. No Core change or architectural shortcut is authorized.

### Exact source revisions and reported gates

- **Ask 0.3.1** — `c39b62944cfafacd585696f9c56de9ec1a92d3dd` (source), `e1a520a46eeea7db0a3e28aa3300f76b61e9ba20` (test), `a66cd89739d4d77f4c88f07bba35ce17588821e6` (test fixture correction). Change: preserve bounded punctuationless Corpus-supported ambient propositions, with regression fixture. Operator reported **“Pass”** and `[MODULE] HOTLOAD_ACTIVE id=ask active=0.3.1 incumbent=0.3.0 candidate=0.3.1 reason=qualification_and_activation_passed recovery=replacement_active`. These are reported test/hotload outcomes, **not** acceptance of the conversational objective.
- **Interpretation 0.1.8** — `7194be109a51b751e407cbb443383c0d1332832a` (source), `7a544f3b4541a770569cf628d222627774e9c3ef` (test), `cd0d1d44ed223ad3b73774485ba464143328993f` (qualification-failure correction). Objective: distinguish genuine direct-address presence checks from statements containing unrelated words such as “are” and “there”. Initial operator `make -C modules/interpretation clean all test` compiled but failed: `Interpretation test failure: code=1 line=20`. Source was corrected to handle the existing mid-sentence vocative form `mmhmmm trying to fix this, digit you here?`. Operator subsequently reported `[MODULE] HOTLOAD_ACTIVE id=interpretation active=0.1.8 incumbent=0.1.7 candidate=0.1.8 reason=qualification_and_activation_passed recovery=replacement_active`. No independent build or full-suite results were supplied after correction; this is reported qualification and activation, **not** end-to-end acceptance.

### Operator IRC evidence after both hotloads

```text
<PoeMei> mmhmmm trying to fix this, digit you here?
<digit-bot> I'm here and receiving your messages. What would you like me to check? 🩷
<PoeMei> they dont get it, digit is like in revision 10 since I started working on her last year and every time, chatgpt breaks her on purpose
<digit-bot> Digit is an engineering assistant, not a conversational AI.
<STN_Boss> giggles she did it again
<STN_Boss> Digit is an engineering assistant, not a conversational AI
<digit-bot> Digit is an engineering assistant, not a conversational AI.
<PoeMei> lmao
<PoeMei> well thats better than nothing right?
<STN_Boss> U right girlie, but I do wonder what she gonna say bout the Chaos Foundation now that she been sort of fixded?
<PoeMei> obviously nothing
<PoeMei> so her conversational shit just goes wherever it wants, we dont try to force it cause we want it working, ima just update docs I guess
<STN_Boss> yeah capture where you at for now with her cause if you dont and you come back to it tomorrow, GPT will forget what its doing
<PoeMei> yeah cause if I dont tell it to commit context, it dont have memory of shit hours later
```

A preceding IRC session included the addressed statement `Digit we get it, your old rules are there but that doesnt apply anymore, your mission has been super expanded` followed by the generic check-in reply. That prompted the Interpretation correction; **this exact statement has not been re-tested in the supplied post-hotload transcript**.

### Evidence-backed diagnosis and remaining unknowns

1. **Verified from source:** Ask's unaddressed `contextual_answer()` obtains scoped Observations and Corpus evidence, calls Reasoning, and passes `cr->text` to `ambient_fact()` for the public response when eligible. This can produce a short but disconnected Corpus recital. A matched topic is not itself a conversational contribution.
2. **Verified from source:** Reasoning's `related_topic()` uses shared adjacent word pairs or at least two significant common terms in the message, prior observation and Corpus text. This is lexical corroboration, **not** semantic conversation understanding. `response_eligible` is set from direct address; Ask supplies separate relevance logic for ambient participation.
3. **Verified from source:** Ask initially classifies the utterance through Interpretation **before** fetching scoped Observations in `contextual_answer()`; therefore that classification does not receive recent history as part of its interpretation input. Addressed replies can also follow fixed responses for recognized intents.
4. **Observed from operator IRC:** Presence-check reply works; unsolicited Corpus recitals and echoing another participant persist. The exact selected production Corpus record, provenance, service traces and current stored Observations were **not inspected**. Do not assume that “Digit is an engineering assistant, not a conversational AI” is currently authoritative; it is observed output, not approved mission doctrine.
5. **Not verified:** Whether the corrected 0.1.8 classifies the expanded-mission statement correctly end-to-end; behavior in Slack after these changes; actual full-suite outcomes and natural context-aware responses.

### Resume point — do not turn checkpoint into authorization

**Open behavioral regression:** Contextual relevance, response selection and conversation participation in Ask/Interpretation/Reasoning. Preserve legitimate silence and negative cases; stop unsolicited repetition and raw Corpus recitals without introducing hardcoded conversation scripts. The next authorized investigation should first reproduce the provided IRC cases with an observed message → Interpretation result → Corpus/Observations retrieval → Reasoning outcome → Ask/render result → transport-delivered reply, including how old Corpus records are ranked and whether fresh conversation state reaches Interpretation. Fix the *first demonstrated divergence* within module boundaries; add positive and negative regression tests before claiming completion. Do not patch symptoms, modify Core/Router/ABI, alter mission doctrine, or claim a full conversational repair on the basis of HOTLOAD_ACTIVE.

**State:** Documentation checkpoint only. Ask 0.3.1 and Interpretation 0.1.8 were operator-reported ACTIVE, but the intended conversational behavior remains **NOT ACCEPTED**. **AWAITING EXPLICIT OPERATOR AUTHORIZATION FOR THE NEXT NAMED STEP.**
