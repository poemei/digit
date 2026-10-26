# Digit Ollama harness — v0.1.0

Operator-authorized **temporary** local language-generation harness for the STN-Labz LLM development effort. This is **not** Digit's knowledge source or replacement for SQLite3, Corpus, Observations, Interpretation or Reasoning.

## Responsibility boundaries

Digit's existing SQLite3 module remains the sole database owner. Conversation and its models retrieve source-attributed Corpus and Observations via the existing `digit.sqlite3.*` ABI services. Ollama is a language facility **after** retrieval, and it may not read or write database tables itself.

Service `digit.ollama.generate` accepts `digit_ollama_request_t` and returns `digit_ollama_result_t` (in `ollama.h`). The request carries the original question, an optional short retrieved evidence excerpt, a token budget, and an explicit `evidence_required` flag. Factual requests require an evidence excerpt; unsupported factual answers must not be emitted as verified facts. The result is always flagged **unverified** and must be checked by Conversation/Reasoning before use.

Only `llama3.2:1b` on `127.0.0.1:11434` is targeted. The request is a synchronous POST to Ollama `/api/generate` with streaming disabled, redirects and proxies disabled, bounded output and timeouts. No credential or third-party hosted service is involved.

The first module revision **does not connect itself to public or private chat**, nor does it automatically send requests. A separate explicit Conversation controller revision is required to consume generated language safely. It does not modify Core, Router, Rictus, or SQLite3.

## Offline test

```sh
make -C modules/ollama test
```

The test compiles against the real Digit ABI and exercises qualification, service registration, invalid requests and service withdrawal without sending network traffic.

## Acceptance

Compilation and qualification do not prove local Ollama connectivity, response quality, correct source grounding, or end-to-end chat behavior. Actual operator testing remains necessary. No automatic learning/training or corpus insertion is authorized.

**AWAITING OPERATOR END-TO-END TESTING AND ACCEPTANCE**
