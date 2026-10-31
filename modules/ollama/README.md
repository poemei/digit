# Digit Ollama Language Generation — v1.1.0

Ollama is a **generation service only**. It no longer registers `/chat`, interprets user questions, invokes Research, or owns conversational routing.

## Service

- Core-mediated service: `digit.llm.generate`
- Local endpoint: `http://127.0.0.1:11434/api/generate`
- Model: `llama3.2:1b`
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
