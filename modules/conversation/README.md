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
