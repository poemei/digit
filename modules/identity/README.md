# Digit Identity & Mission — 1.1.0

Read-only approved identity/mission record and Core-authoritative General Orders. The module is registered through Digit's existing ABI 1.5, without any IRC or shared ABI changes.

## Authority

- Mission and operational identity: installed `/opt/digit/modules/identity/identity.conf` approved record.
- Three General Orders: fetched from Core's new read-only `digit.root.orders.read` service, which reads the validated Root identity; no duplicate orders or module authority to modify them.
- Core controls read service, remains the source of truth, and does not classify natural-language text.
- Identity module classifies questions and returns exact records through `digit.identity.answer`. Ollama's chat controller first invokes that service. Non-identity questions fall through to LLM; identity service errors fail closed.
- No modification to IRC or the shared ABI. Core modification was separately authorized by the Director for this task.

## Supported IRC requests

`/identity mission`, `/identity who`, `/identity orders`, `/identity first general order`, `/identity second general order`, `/identity third general order`.

Natural-language questions through the usual chat route: `digicat: what is your mission?`, `digicat: who are you?`, `digicat: what is your 1st general order?` and similarly `2nd`, `3rd`, `first`, `second`, `third`. Responses are deterministic and do not invoke Ollama generation.

## Qualification

```sh
make clean && make && make test
make -C modules/identity clean && make -C modules/identity && make -C modules/identity test
make -C modules/ollama clean && make -C modules/ollama && make -C modules/ollama test
```

These commands require the repository's existing external ABI checkout. Install the revised Core **only under Director control** before installing/hotloading Identity v1.1.0 and Ollama v1.0.5. Qualification GREEN, hotload and client behavior are separate acceptance gates. Verify the natural-language questions and ensure unrelated chat still reaches Ollama.

Source committed; host build, test, activation and live routing have not yet been verified.
