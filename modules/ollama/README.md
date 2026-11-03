# Digit Ollama Language Generation — v1.1.2

Ollama is a **generation service only**. It no longer registers `/chat`, interprets user questions, invokes Research, or owns conversational routing.

## Service

- Conversation service: `digit.llm.generate` (existing callers unchanged).
- Engineering service: `digit.llm.engineering.generate` (explicit callers only).
- Local endpoint: `http://127.0.0.1:11434/api/generate`
- Conversation model: `llama3.2:1b` (unchanged).
- Engineering model: `qwen2.5-coder:3b`, loaded on demand, with `keep_alive=0`, a 160-token generation cap, and a bounded engineering timeout. The model must already be present in Ollama; the module never pulls or installs it.
- Input/output contract: `include/digit_llm.h`
- No credential file required.
- Identity, Research and conversational direction are handled by their own modules.

Digit Ask (`modules/ask`) registers `chat`. Ask invokes Research's `digit.research.answer.v1` service and uses Ollama's generator **only for unclaimed requests**. Research itself owns research intent and evidence. IRC and Core remain transport/router only.

## Build and operator-controlled activation

```sh
make -C modules/ollama clean all test
sudo make -C modules/ollama install
```

**Route transition:** The previously installed Ollama v1.0.9 owns `chat`. The updated v1.1.0 release relinquishes that route on hotload; `modules/ask` v1.0.0 then claims it. If Ask is activated first it will conflict with the old route owner. The operator must verify activation of Research v1.3.0, Ollama v1.1.0 and Ask v1.0.0 in the running Core. A temporary gap in `chat` is possible if the transition is not coordinated.

Native compilation, qualification, installation, activation and client acceptance remain unverified in this source edit. Existing historical acceptance receipts do not qualify this revision.

## History

- v1.0.4 registered `chat` locally to make general conversation possible.
- v1.0.5 delegated identity and General Orders questions to the authoritative Identity module.
- v1.0.6 preserved peer-directed say/tell/ask messaging.
- v1.0.7–1.0.9 temporarily routed Research from inside Ollama and added concise response behavior. Those responsibilities have now moved to the proper owners.
- v1.1.0 retains generation only; Ask owns the conversational route and peer orchestration.

## 1.1.1 — Remove unsolicited IRC recipient bias

The inference provider's global system prompt previously included a named IRC participant and examples of addressing that participant. Those instructions were applied indiscriminately, including to ordinary research answers. The global prompt now leaves recipient choice and delivery to the calling controller and does not embed names or specific relay examples. Explicit peer commands remain handled by Ask. Protocol selftests check that an unrelated research question does not contain the previous participant name or examples. This change is source-only and is not live-validated.

## 1.1.2 — On-demand engineering model

The existing generation service continues to use the original lightweight model and contract. The new named service `digit.llm.engineering.generate` accepts the **same** `digit_llm_request_t` and `digit_llm_response_t` byte layouts, but selects `qwen2.5-coder:3b` for explicit engineering operations. It uses the same local Ollama daemon; there is no global model configuration switch, Core change, or second Ollama server. Engineering requests set `keep_alive=0` so Ollama may unload the coder model on completion. They use short output and a longer request timeout because the host has two vCPUs and no observed GPU. They fail explicitly if the model or service is unavailable; conversation never silently substitutes for engineering.

GitHub `/github owner/repo inspect file` requires the companion GitHub module 1.0.7 to select the new service. If GitHub 1.0.7 is installed without Ollama 1.1.2, its source-evidence fallback is expected; it must not claim to have analyzed code. Operator-controlled qualification and activation remain required. Native builds and live operation are not yet verified for these changes.
