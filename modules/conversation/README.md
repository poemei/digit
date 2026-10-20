# Digit Conversation v1.0.3 — SQLite context integration

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
