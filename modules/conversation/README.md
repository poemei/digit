# Digit Conversation v1.0.1 — Interpretation ABI integration

Conversation registers `digit.conversation` and `digit.conversation.listen`, preserving the existing IRC and Core ABI routing. Both routes pass incoming message text into the independently registered `digit.interpretation` ABI service. There are no direct shared-library calls between modules.

The module validates inbound envelopes, the service status, and the exact Interpretation result layout. Successful interpretation produces zero response bytes; passive listening and direct input both remain silent until authorized reasoning, participation and response generation exist. An absent Interpretation service reports `NOT_FOUND` rather than pretending processing succeeded.

**No LLM, Ollama, external inference provider, scripted responses, or unauthorized Core changes.**

## Existing hotload lifecycle

The manifest remains `id=conversation`. The descriptor patch revision increases from 1.0.0 to **1.0.1**, permitting the existing Core hotload replacement process to evaluate a new binary (SHA-256 change and strictly newer descriptor version). Core stages the candidate, qualifies it, checks authorization, stops and replaces the incumbent, and rolls back where needed. Neither Core nor IRC needs to be modified or restarted by the design.

Build with `make`. The local `make test` target currently verifies **compilation only**. Core's descriptor qualification executes dispatch tests with a deterministic mock Interpretation service; passing them is not evidence of live IRC-to-Interpretation behavior. An operator must verify qualification, hotload, logs, and actual message behavior.

No database schema changes. No conversational generation is claimed. Installation/deployment remains operator-controlled.
