# Digit Ask — native conversation controller 1.0.1

Ask owns the registered `chat` route (ordinary directed conversation). It does not directly call external networks, classify research intent, access Corpus tables, or generate language.

- Authoritative Identity first through Core service `digit.identity.answer`.
- Research's `digit.research.answer.v1` service next; **Research alone owns research intent** and either returns a complete answer, an unavailable/denied status, or NOT_APPLICABLE.
- Ollama's `digit.llm.generate` service only for requests Research does not claim.
- Peer-directed messages maintain the previous Ask/Tell/Say-to participant behavior.
- Every output stays within Core's 512-byte routed-reply limit.
- Core/IRC/shared ABI unchanged.

**Activation ordering matters:** Ollama v1.1.0 must relinquish `chat` before Ask 1.0.0 is activated. Research 1.3.0 must already be active. Updating files is not proof hotload/activation occurred. Temporarily losing `chat` while switching ownership is possible without a coordinated operator-controlled transition.

```sh
git pull
make -C modules/research clean all test
make -C modules/ollama clean all test
make -C modules/ask clean all test
sudo make -C modules/research install
sudo make -C modules/ollama install
sudo make -C modules/ask install
```

These source changes are not automatically installed or qualified. The operator controls runtime activation. Verify: normal greeting, authoritative identity, next SpaceX launch, live-information failure, peer-directed ask/tell/say, and an unclaimed technical question.

**State:** Source committed; compilation, qualification, actual activation and operator acceptance pending.

Ask v1.0.1 relays Research-owned unavailable evidence instead of replacing it with a generic verification refusal. Service-call success remains distinct from verified research.
