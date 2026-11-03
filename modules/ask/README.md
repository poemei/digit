# Digit Ask — native conversation controller 1.0.7

Ask owns the registered `chat` route (ordinary directed conversation). It does not directly call external networks, classify research intent, access Corpus tables, or generate language.

- Authoritative Identity first through Core service `digit.identity.answer`.
- Explicit `research <query>` or `search <query>` requests use Research's `digit.research.answer.v1` service. Research retains ownership of acquisition and result formatting. Its dedicated `/research` command remains available.
- Ordinary conversation (including `Hello`) and explicitly directed peer messages use the existing lightweight `digit.llm.generate` service. Ask never calls the separate engineering model.
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

## 1.0.5 — Direct search results to IRC

Operator-approved KISS path: incoming ordinary question → Research service → one DuckDuckGo search → formatted search results → ordinary IRC response. Ask no longer sends research results to the language inference service, asks the model to infer dates or locations, or manufactures an answer from result snippets. The user's exact question is passed unchanged to Research. Research unavailable, empty and denied statuses return directly without invoking language inference. The pre-existing authoritative Identity response and explicitly addressed peer messages keep their previous handling. Tests cover exact query handoff, verbatim results, no inference, denial, unavailability, identity and explicit peer routing. No Core, IRC transport or shared contract changes; native qualification and live operation remain unverified.

## 1.0.6 — Restore ordinary conversation routing (2026-11-03)

**Observed operator failure:** `Hello` returned an unrelated applicant privacy-policy headline. Source inspection established that Ask 1.0.5 routed **every ordinary question and greeting** directly to Research, because it selected Research whenever the message was not a peer command. This was a controller-routing defect, not a coder-model failure.

**Correction:** After authoritative Identity, ordinary `chat` messages invoke the existing lightweight conversational service. Only explicit `research <query>` or `search <query>` messages invoke Research through Ask; the independently registered `/research` command is unchanged. Explicit peer-directed `ask`, `tell`, and `say ... to` messages retain their original chat-model and peer-send path. An unsolicited leading IRC nickname in an ordinary generated reply is removed before delivery, without sending anything to that participant. Engineering inference remains the separate GitHub/engineering service.

**Examples:**

```text
Hello                         -> lightweight conversation
What is PHP?                  -> lightweight conversation
research PHP 8.5 updates      -> Research evidence
search next SpaceX launch     -> Research evidence
/research next SpaceX launch  -> Research's own command
/github poemei/digit inspect src/digit.c -> engineering model
```

**Validation:** Source and offline regression tests cover conversational greetings, ordinary technical questions, explicit Research, unavailable/denied research, authoritative Identity, peer messages, and unwanted nickname prefixes. Native compilation, test execution, installed version, and live IRC acceptance remain unverified until operator receipts. No Core, ABI, Ollama, Research, or GitHub source files were changed for this correction.

## 1.0.7 — Standalone greeting regression recovery (2026-11-03)

**Operator evidence:** After Ask 1.0.6 went live, `Hello` returned `Language inference failed; no answer was generated.` Explicit `research PHP 8.5` continued to work. The previous Ask change routed basic greetings through the lightweight Ollama service without establishing its availability, so it broke historically reliable common greetings. Older repository history includes a dedicated greeting service; that is separate from, and not a substitute for, proving current inference service health.

**Fix:** Standalone case-insensitive `Hello`, `Hi`, and `Hey` (with surrounding whitespace and basic punctuation) receive direct small replies in Ask after the authoritative Identity service, without calling Ollama or Research. A message that merely starts with a greeting but contains a substantive request remains on the original conversation route. No changes to Research, GitHub, coder inference, Core, or peer transport. Offline regression covers greetings under simulated inference failure, mixed technical requests, and existing controller behavior.

**Important limitation:** This restores basic greeting behavior; it does **not** demonstrate that the existing `digit.llm.generate` inference provider is healthy for other conversation. The live cause of that service failure requires its server-side Ollama model inventory or module logs. Native build, test execution, and operator acceptance remain pending.
