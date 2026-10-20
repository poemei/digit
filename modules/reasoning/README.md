# Digit Reasoning 0.1.0

A deterministic, evidence-constrained reasoning evaluation module under Digit's ABI 1.5.

Service: `digit.reasoning.evaluate`. Receives Interpretation features and model-supplied Corpus evidence with retrieval status. Returns a structured decision: abstain, supported evidence available, clarification required, or conflicting evidence, with source IDs and uncertainty signals.

This is not a scripted answer engine. The module does not access the database, invoke transports, produce prose, self-authorize actions, mutate records, or claim to have understood semantic meaning unsupported by the existing lexical Interpretation service. Corpus verification states are treated conservatively. A missing or unsuccessful retrieval cannot be converted into an answer.

This is a foundational reasoning stage, **not general intelligence**. The existing Interpretation and View generation limitations remain. DSD sees its invocation from Core dispatch. Conversation v1.0.7 invokes this service after Interpretation and model-owned Corpus retrieval. Reasoning does not register as a replacement for Conversation or supply generated prose.

Qualification executes inside Digit Core; a standalone `make test` checks compilation only.


## v0.1.1 evidence eligibility

Verified record metadata alone is no longer considered enough. Evidence must contain the interpreted subject as a whole lexical term, carry multiple content words, and include explanatory punctuation. Isolated labels such as `PHP` and unrelated snippets are rejected even when Corpus reports a match. This is a necessary quality gate, not semantic entailment or factual verification; no automatic language synthesis is claimed.


## v0.1.2 — honest uncertainty outcomes

Reasoning now distinguishes `UNKNOWN` (a completed query supplies no adequate evidence), `CLARIFY` (an isolated ambiguous term or unresolved meaning), and `FAILURE` (retrieval did not complete). These states are distinct from an evidence-supported result. A successful lookup with no matches is not a database failure. An isolated name with no matches yields unknown; a bare term with hits is not automatically permission to answer on an assumed topic. This remains a bounded heuristic rather than full contextual understanding.


## v0.1.4 — source-owned DSD evidence

The Reasoning service now records its own request-correlated disposition through the existing Core DSD interface, including `KNOWLEDGE_UNKNOWN`, `CLARIFICATION_REQUIRED`, `EVIDENCE_AVAILABLE`, and `RETRIEVAL_FAILED`. It records the source as `modules/reasoning/reasoning.c`, rather than relying on Conversation to report the decision. Trace recording is inert if no request trace is bound and does not change the actual reasoning outcome. Under the current DSD event prioritization, the Reasoning decision precedes Conversation's matching disposition and can be attributed directly to Reasoning. This is instrumentation, not new semantic reasoning.
