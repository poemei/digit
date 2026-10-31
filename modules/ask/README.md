# Digit Ask — native conversation controller 1.0.4

Ask owns the registered `chat` route (ordinary directed conversation). It does not directly call external networks, classify research intent, access Corpus tables, or generate language.

- Authoritative Identity first through Core service `digit.identity.answer`.
- Research's `digit.research.answer.v1` service next; **Research alone owns research intent** and either returns a complete answer, an unavailable/denied status, or NOT_APPLICABLE.
- The replaceable `digit.llm.generate` service supports both unclaimed questions and evidence-grounded responses to research questions.
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

Ask 1.0.2 removes the early return that treated unverified search listings as the user's final answer. Bounded source evidence is provided as context to language inference. Safety-denied requests remain denied. Provider or search failures are explained using actual failure context, without pretending live facts were verified. No Core, ABI or transport changes; build, qualification, activation and operator testing remain outstanding.

## 1.0.3 — Ordinary reply/IRC recipient isolation

An ordinary chat answer is not authorization to address a channel participant. When the replaceable inference service returns a leading IRC nickname with a colon, Ask strips that unintended recipient prefix before normal reply delivery; the generated content is not dispatched through the peer send service. Explicit `ask`, `tell`, and `say ... to` peer requests retain existing controller-owned routing and send behavior. A regression supplies the observed `rictus:` prefix from language generation and verifies zero peer sends. If stripping yields empty content, Ask rejects it rather than publishing an IRC address. This is a source-level guard; it is not evidence of live bot-loop prevention or satisfactory factual research answers. Build, qualification and operator acceptance remain pending.

## 1.0.4 — Complete evidence handoff to language inference

Ask now constructs a question-first inference request with the Research
retrieval timestamp and the source-associated result text, copying as
much evidence as the unchanged 2048-byte inference request permits.
The previous arbitrary 1300-byte source truncation and disclaimer-heavy
wrapper have been removed. Ask does not invent information, decide which
subjects can be searched, crawl other web pages, change the shared ABI, or
select IRC participants from inferred content. Peer-directed commands,
identity routing and safety denials retain their existing boundaries.

Qualification and live question-answer behavior remain pending. When
search-result titles and excerpts omit a requested fact, inference cannot
reliably supply it from the evidence alone.
