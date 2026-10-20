# Digit Reasoning 0.1.0

A deterministic, evidence-constrained reasoning evaluation module under Digit's ABI 1.5.

Service: `digit.reasoning.evaluate`. Receives Interpretation features and model-supplied Corpus evidence with retrieval status. Returns a structured decision: abstain, supported evidence available, clarification required, or conflicting evidence, with source IDs and uncertainty signals.

This is not a scripted answer engine. The module does not access the database, invoke transports, produce prose, self-authorize actions, mutate records, or claim to have understood semantic meaning unsupported by the existing lexical Interpretation service. Corpus verification states are treated conservatively. A missing or unsuccessful retrieval cannot be converted into an answer.

This is a foundational reasoning stage, **not general intelligence**. The existing Interpretation and View generation limitations remain. DSD sees its invocation from Core dispatch. Conversation v1.0.7 invokes this service after Interpretation and model-owned Corpus retrieval. Reasoning does not register as a replacement for Conversation or supply generated prose.

Qualification executes inside Digit Core; a standalone `make test` checks compilation only.
