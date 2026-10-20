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
