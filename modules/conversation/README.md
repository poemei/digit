# Digit Conversation v1.2.3 — SQLite context integration

**AI** is Ignoring Company Version Policy

Conversation keeps the existing `digit.conversation` and `digit.conversation.listen` services, and the existing IRC/Core routing. It invokes Interpretation and, for authenticated messages in `#stn-labz`, writes and retrieves context through the SQLite3 module's registered ABI services.

Context records originate from actual persisted IRC Observations. Conversation submits the interpreted utterance category and original message, then reads the bounded context history before any source-grounded response logic. Context failures are returned as failures, not disguised as success. Both services are owned by SQLite3; Conversation does not open a database or execute SQL.

The retained categories `utterance.question` and `utterance.statement` describe observed speech, not asserted truths. Their original Observation event IDs provide provenance. Retrieval remains bounded to eight records, ordered oldest-first, and filters expired records.

This integration **does not implement general conversation understanding, topic resolution, sustained reasoning, or unrestricted natural-language generation**. The existing narrow PHP diagnostic remains and is explicitly not a substitute for those capabilities. No LLM, Ollama, Core modifications, IRC modifications, or schema migration.

## Qualification and operator acceptance

- Descriptor version **1.0.3** (prior 1.0.2), following the revision doctrine.
- `make` builds this module; `make test` currently only builds it.
- Descriptor qualification exercises Interpretation dispatch and context service calls with deterministic mocks. This is not live SQLite or IRC acceptance.
- Validate SQLite3 v0.4.5 context registration, actual persisted context rows, provenance, and Conversation's readback after operator-controlled hotload.
- Do not claim full conversational participation based only on ACTIVE status.


## Runtime persistence correction (v1.0.4)

Conversation no longer creates a second context record or attempts to reconstruct an Observation by text and account. SQLite3 creates Observation-linked context atomically when the existing Observation service writes the record; Conversation reads a bounded context history. This change does not add general conversational understanding or automatic participation. Live acceptance requires SQLite3 v0.4.6 followed by Conversation v1.0.4.


## DSD request tracing (v1.0.5)

Conversation records request-correlated service boundary outcomes for its Interpretation invocation, conditional SQLite3 context-recent invocation, and final participation disposition. Events use the existing thread-bound DSD trace ID; without a bound trace, tracing is inert. Only status, source line, operation and bounded counts are emitted; message contents and accounts are not included. This exposes *executed* stages, not fabricated downstream reasoning. The existing non-generative Conversation behavior is unchanged.


## Evidence-to-Reasoning dispatch (v1.0.7)

After Interpretation, Conversation submits a candidate lexical subject term to the established `digit.sqlite3.corpus_search` model and supplies the returned evidence, success/failure status, and Interpretation results to `digit.reasoning.evaluate`. It does not access SQLite directly. DSD records both service boundaries. Reasoning returns a bounded provenance-aware outcome rather than a scripted answer. Until a separately authorized View/generation capability exists, a general request may still produce `NO_GENERATOR`; this must not be misrepresented as reasoning failure.

Requires Reasoning v0.1.0 to be active before Conversation v1.0.7.


## Evidence-bound response View (v1.0.8)

Conversation can now pass a Reasoning-selected, operator-verified Corpus reference to its dedicated View component (`response_view.c`). The View renders a complete first source sentence with the Corpus ID, only for a direct private request. It cannot invent a definition, repair code, or claim an original synthesis; if evidence is missing, unverified, mismatched, truncated or unsuitable, it abstains.

The View is not a substitute for full semantic synthesis or a general language generator. The implementation is a limited, source-faithful response path to replace unconditional silence where selected evidence supports a bounded response. No Core, IRC, SQLite3 schema, or Reasoning ABI changes. DSD receives `response.view` statuses `RESPONSE_COMPOSED` or `RENDER_ABSTAIN`, and final `RESPONSE_RETURNED` or `RESPONSE_ABSTAIN`.

The descriptor is Conversation v1.0.8. The module Makefile builds both `conversation.c` and `response_view.c`. Qualification includes success, unverified record, mismatched source ID, incomplete sentence, verification-required decision, and output-capacity cases. Module activation, runtime qualification, and user acceptance remain operator controlled.


## Response gating diagnostics (v1.0.9)

The request path now records a `response.view` DSD event for every Reasoning result, including `CHANNEL_OBSERVATION`, `OUTPUT_UNAVAILABLE`, `RETRIEVAL_UNAVAILABLE`, `REASONING_ABSTAIN`, `CLARIFICATION_REQUIRED`, `EVIDENCE_NOT_RENDERABLE`, and `RESPONSE_COMPOSED`. A skipped View must not leave Core DSD interpreting an unrelated IRC `NO_RESPONSE` as the root cause. The View may present a bounded operator-verified Corpus excerpt with an explicit Corpus ID, quotation framing, and ellipsis for truncation. This does not implement general semantic synthesis or promise an explanation from retrieval alone. Core remains unchanged; Core's current six-line summary may still need to incorporate the new stage dispositions separately.


## Evidence sufficiency correction (v1.0.10)

Conversation preserves the model-to-Reasoning-to-View path. Reasoning v0.1.1 must establish minimum source substance and lexical relevance before it authorizes display; the View independently rejects isolated words or fragments without explanatory structure. An inadequate match reports `INSUFFICIENT_EVIDENCE` rather than being presented as an explanation. A source excerpt remains identified as an excerpt, not original synthesis. These are conservative necessary checks only; substantive semantic correctness and genuine multi-source explanation remain unimplemented.

Requires Interpretation v0.1.2 and Reasoning v0.1.1 for the updated interpretation/evidence contracts.


## v1.1.1 — verification-state consistency

The Response View accepts the same explicit stored statuses as Reasoning v0.1.5: `operator-verified` and `verified`. It does not rewrite either label; `pending` and `unverified` remain ineligible. Qualification tests the accepted and rejected statuses. The View still presents attributed Corpus excerpts rather than independently authoring an explanation.


## v1.1.2 — explicit partial response

Conversation and its Response View accept Reasoning's `PARTIAL` outcome. The View identifies the verified Corpus record and explicitly labels its text as *not a full explanation*. Reasoning's `UNKNOWN`, `CLARIFY`, and retrieval-failure outcomes remain active; uncertain evidence is not automatically presented as a complete answer. Added qualification cases check permitted partial evidence and refusal of unverified content. Core and IRC remain unchanged.


## v1.1.3 — addressed IRC channel participation

Conversation registers distinct handlers for passive `digit.conversation.listen` and interactive `digit.conversation`. Passive channel traffic remains observable without a mandatory reply. When IRC routes an addressed channel message through Core's `digit.message` route, Conversation permits the same source-grounded View outcomes as a private message, including clarification and honest uncertainty. Channel context lookup remains enabled, and structural code diagnostics cannot overwrite an already rendered response. Qualification fixtures exercise channel passivity and addressed-channel uncertainty rendering. No Core or IRC code was changed. Live channel routing and runtime participation remain subject to operator acceptance.


## v1.1.4 — Core-owned identity access

Questions referring to Digit's identity, lineage, mission, or mother now read a bounded value using the Core-owned `digit.core.identity.read` ABI service. The conversation module does not duplicate Core identity or rewrite protected Core facts. A question about a creator is explicitly distinguished from Core's mother relationship: the authoritative identity does not declare a creator. The response says so instead of inventing one. When the read-only service is unavailable, Digit reports the technical failure, not a lack of personal knowledge. This is separate from Corpus retrieval and Reasoning. Qualification fixtures now cover representative self, mission and creator questions; live service integration has not been executed here.


## v1.1.5 — participation after Interpretation

The Conversation listener now makes a bounded participation decision after Interpretation rather than automatically suppressing every unaddressed channel message. Recognizable interrogative invitations, including `can you tell me about...` without a question mark, may receive an evidence-constrained response. Stray question marks or unrelated remarks do not by themselves authorize one. Directly addressed contact-only messages receive an acknowledgment rather than a knowledge-clarification response. Qualification cases cover the observed channel examples with mock Interpretation and Reasoning dependencies, not a live IRC integration. This does not yet provide semantic discourse understanding, follow-up resolution, or unsolicited topical contributions beyond the bounded invitations.

## v1.1.7 — shared Socials for nonfamily contact

A directly addressed, nonverbal social contact may request the existing
`digit.socials.select` category `cats`, regardless of who sent it.
Family relationship identification stays entirely separate and retains IRC's
verified account-and-nickname rules. Normal knowledge questions continue to
Corpus/Reasoning. Unaddressed channel messages do not automatically trigger
an unsolicited reply. If Socials is missing, optional expression failure is
not recast as a knowledge-clarification question. The module's qualification
fixtures cover an addressed smile, ordinary punctuation, and `?` contact.

Socials v0.2.4 may enrich this category from a short verified Corpus record,
so the same expression vocabulary can serve Momma, Auntie, Jess, or an
unfamiliar visitor. This is **bounded nonverbal interaction**, not general
social reasoning or spontaneous conversational participation. The actual
Corpus record taxonomy has not been inspected or confirmed.

## v1.1.9 — Context-before-reasoning retrieval

Conversation now reads recent channel observations before evidence selection, then
tries more than the final lexical term against the existing Corpus search service.
For questions containing unresolved references, earlier observed utterances can
supply additional retrieval candidates if the current words yielded no results.
These are **search candidates only**; a prior statement is not authenticated
knowledge, and the implementation does **not** equate `she`, `he`, or `they`
with a fixed person. Existing Reasoning and View verification rules still
govern emitted claims. The mock qualification case for `does she understand
cat talk?` checks that available verified feline evidence can be retrieved
without blindly emitting a clarification request.

Limitations: the existing context ABI contains only observation text and source
ID, without resolved discourse entities or conversational subject scores.
Thus these changes improve context-aware retrieval but do **not** implement
full coreference resolution. Live integration and the deployed Corpus contents
must be tested by the operator.

## v1.1.10 — Verified candidate selection across question terms

Conversation no longer chooses the first nonempty full-text search result.
It searches each non-grammatical question term (including capability verbs),
considers verified results returned by the existing Corpus ABI, and retains
its highest lexical-overlap excerpt for the established Reasoning and View
services. The existing model/ABI limits still apply (up to three hits per
individual search). The qualification mock includes a direct Digit feline-
capability question where an irrelevant early match must not displace a
stronger later match. This is not a proof of full semantic understanding,
pronoun resolution, or the actual deployed Corpus contents.

## v1.2.3 — GRU generation handoff

After the existing Interpretation, Corpus Search, Reasoning and deterministic View succeed, Conversation calls `digit.gru.generate` using a bounded prompt containing the evidence-backed View answer. The registered `gru_bridge` service posts to `http://127.0.0.1:8765/generate`. A missing bridge, rejected HTTP/model response, timeout or failed candidate validation retains the original View answer; an optional module never blocks the established reply. The candidate acceptance policy is intentionally narrow: the verified View statement must be preserved verbatim and only a small approved conversational introduction may precede it. This does not yet enable open-ended paraphrasing; a robust independent factual-equivalence validator would be needed before allowing freer generation. No inference, SQL changes or Core/IRC changes were introduced. The existing approved-pair learning path remains separate. Live acceptance needs the GRU API schema expected by `gru_bridge` (including `model=stn-labz-gru`, `text`, `generated_bytes`, and `experimental`).


## v1.2.9 — GRU question-format alignment (2026-10-24)

Conversation now submits the **original user message** as the GRU generate prompt instead of prepending natural-language instructions, evidence metadata, and retrieval labels. The word-GRU sampler itself supplies its learned question/answer context markers. Conversation requests **32 maximum words**, matching the Windows GO diagnostic generation budget; the bridge retains its established default seed (913), temperature (0.65), and repetition penalty (0) because the ABI request leaves those fields unset.

The change is deliberately confined to `modules/conversation/conversation.c`. It does not alter Core, Interpretation, Reasoning, the evidence-backed View, SQLite persistence, the GRU bridge ABI, the HTTP service, checkpoint selection or deployment. A generated reply remains **unverified language**: verified excerpts are still protected by the existing exact source-quotation acceptance check, with fallback retaining the evidence-backed response when the GRU cannot preserve it. The existing sensitive metadata output checks remain.

Qualification's deterministic GRU stub now verifies that the prompt equals the user's original message, the requested word budget is 32, a response missing a required verified source quotation is rejected, and one containing that quotation is accepted. These tests establish request formatting and guard behavior only, not live GRU fluency or factual correctness.

**Operator gate:** Build and run module qualification, then compare matched prompts on Windows and IRC under the same checkpoint, executable revision, seed, temperature and word limit. Checkpoint equality alone does not prove executable equality. This revision does not install, hotload or activate the module.

## v1.2.10 — retrieval non-attempt versus failure (2026-10-24)

Conversation records `RETRIEVAL_SKIPPED` when the interpretation yields no eligible substantive Corpus search terms; it no longer reports `RETRIEVAL_UNAVAILABLE` without attempting a query. Distinct `NO_CORPUS_MATCH` and `RETRIEVAL_UNAVAILABLE` results remain for successful-but-empty and attempted-but-unsuccessful retrievals. When Reasoning abstains after no search subject was selected, Conversation records `NO_SEARCH_CANDIDATE` rather than falsely describing the Reasoning/View path as failed evidence retrieval.

The correction does not send grammatical/discourse words to Corpus, does not alter Core identity authority, and does not make probabilistic GRU text into verified evidence. See `docs/CORE_DSD.md` for the complete trace diagnosis and evidence boundaries. Operator validation remains required.

## v1.2.11 — auditable GRU generation gates (2026-10-24)

The Conversation → GRU service now emits request-correlated, bounded Core DSD events for prompt bytes, requested word budget (32), generator service failure, malformed result, output length mismatch, control characters, missing required verified quotation, metadata leakage, and structurally accepted **unverified** text. It never logs raw messages or model output, and does not modify sampling, learning, approved data, language qualification, or factual verification policy.

The purpose is to expose exactly how an irrelevant reply such as `The same group.` can be accepted: the existing checks establish valid byte format and optional source-quotation fidelity, **not semantic relevance**. Core DSD exposes `semantic_check=NOT_PERFORMED` in its report. This is diagnostic instrumentation only; a human must judge substantive quality and accept any later model correction. Module descriptor increments from v1.2.10 to **v1.2.11** to meet Core's existing hotload version gate.

## v1.2.12 — local Ollama language harness (2026-10-26)

Conversation now calls the established `digit.ollama.generate` ABI instead of retired GRU services. The existing Interpretation, SQLite3 Corpus/context retrieval, Reasoning, and evidence-backed View continue to own factual processing. Generated output remains unverified; for evidence-backed replies, a candidate must preserve the verified source excerpt verbatim or the View response is retained. UNKNOWN, CLARIFY, and FAILURE do not receive unsupported factual completions. Social/nonfactual turns may invoke the optional harness when the existing participation logic permits it. Missing Ollama service and malformed output fall back without altering Core, SQLite3, or Rictus.

The Makefile uses `../ollama/ollama.h`, and the module descriptor is `1.2.12`. Conversation qualification includes a deterministic Ollama service mock, request budget, unavailable service, accepted output, and rejected missing-evidence quotation. These checks alone do not establish complete malformed-output coverage or live language quality. `make test` builds the shared module; authoritative qualification executes through Digit Core. This GitHub source correction has **not** been compiled, qualified, installed, hotloaded, or tested against live IRC/Slack in this session.

**AWAITING OPERATOR END-TO-END TESTING AND ACCEPTANCE**

## v1.3.0 — qualification correction and version rollover (2026-10-26)

The prior v1.2.12 descriptor exceeded the maximum revision allowed by 20260906.0 Version Numbering Doctrine. The next valid release identity is **v1.3.0**, now reflected in the Conversation descriptor and Makefile.

The Ollama qualification fixture previously required a nonempty factual reply for `what is the Chaos Foundation?` while its own mock Reasoning outcome was ABSTAIN with no eligible Corpus evidence. This assertion contradicted the production evidence policy and could cause `candidate_qualification_failed`. The fixture now expects successful processing with no generated factual reply under that deliberately evidence-free condition. The separate direct Ollama service tests still check the request contract, missing verified quotation rejection, valid marked-unverified output, unavailable service, and malformed responses.

No Core, SQLite3, Reasoning, Router, or Ollama module changes were made. Repository source changes alone do not establish successful compilation, module qualification, hotload, or live client acceptance.

**AWAITING OPERATOR END-TO-END TESTING AND ACCEPTANCE**

## v1.3.1 — Ollama-first addressed conversation and passive silence (2026-10-26)

Conversation now invokes the local `digit.ollama.generate` service **first** for nonempty, directly addressed messages, mirroring Rictus's IRC → LLM → reply orchestration. Local generated replies remain marked unverified. If the service is unavailable or its output violates the existing ABI/output checks, Conversation falls through to its existing deterministic interpretation, Corpus/Reasoning, and View paths. Core identity questions retain their authoritative Core response. No database writes or training are authorized by generated text.

The passive IRC listener now observes without emitting unsolicited replies, even when another participant asks an ordinary question or sends a social expression. Direct requests and passive observations continue to use their established distinct services. Qualification fixtures updated for the intentional addressed Ollama reply and passive no-response behavior.

No Core, SQLite3, Rictus, or Ollama changes were made. Source is committed but local compilation, runtime Core qualification, and live chat acceptance are not confirmed by this update.

**AWAITING OPERATOR END-TO-END TESTING AND ACCEPTANCE**
