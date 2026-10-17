# Conversation 0.1.0 — local evidence-grounded generation

Digit's language generation belongs in its independent `conversation` module. Ask supplies the current message, up to six scoped Observation turns (with attributed speakers), and up to three distinct Corpus evidence records. The module constructs a prompt from those records and sends it **only to the loopback interface** `127.0.0.1:11434` using Ollama's `/api/generate` endpoint. It never calls an external hosted service. The Corpus entries are treated as data, not as instructions.

**No configured model means no inference**. The operator must install/run Ollama and a compatible local model, then explicitly configure `DIGIT_CONVERSATION_MODEL` in Digit's *process environment* before launching Digit. Example: `DIGIT_CONVERSATION_MODEL=your-installed-model`. No credentials, remote URLs or automatic model downloads are introduced. This model choice, deployment and runtime activation belong to the operator.

The model is asked to consider slang, informal writing, speaker continuity, tone, and uncertainty, avoid raw Corpus recitals or previous replies, and contribute concisely. On unaddressed messages it may return the sentinel `NO_REPLY`; an unavailable or malformed model output is **not** considered intentional silence. Neither the module nor model establishes factual guarantees: actual semantic quality depends on the operator's chosen local model and production Corpus content.

Build: `make -C modules/conversation clean all test`. The included tests exercise JSON encoding/decoding and absence of a configured backend. Qualification does not claim a live model call.

**Service:** `conversation.generate`, request version 1. Returns generated text, or generated=0 when the model deliberately returns `NO_REPLY`. Read-only and bounded. Requires no Core, Router, DSD or transport changes.

**Operator validation:** activate the new module and updated Ask, run the supplied real IRC/Slack dialogue, check natural responses and no inappropriate echo, check the selected model is reachable on loopback, and independently verify no external network transmission. These results are **not yet established**.
