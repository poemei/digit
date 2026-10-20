# Digit Conversation v1.0.2 — evidence-bound participation

Conversation retains `digit.conversation` and `digit.conversation.listen`, receiving messages through the established Core and IRC service routing. It invokes `digit.interpretation` via the Core ABI. On public-channel traffic it can also consult `digit.sqlite3.observation_recent` without altering observations.

## First bounded participation capability

A structural diagnostic recognizes one demonstrable code error: a PHP `if (...)` statement posted following a visible HTML `</div>` boundary, without a PHP opening tag intervening in the observed channel context. In that case Conversation returns a short technical diagnosis; IRC already sends nonempty responses as PRIVMSG. It never claims the code was fixed. It avoids replying when evidence is absent, when PHP was already opened, or when the output buffer is insufficient.

This is a **specific syntax-analysis capability**, not general semantic conversation. It does **not** solve all PHP defects, comprehend arbitrary dialogue, synthesize knowledge generally, or satisfy the full mission requirement. In particular the bounded recent context can omit an earlier PHP opening tag, so human acceptance must assess false positives. Source parsing and contextual evidence are necessary before this diagnostic is enabled operationally.

Conversation has no Ollama, LLM, neural inference or external inference dependency. No Core or IRC source changes were made. This work does not replace Digit's intended autonomous reasoning architecture with canned chatbot responses.

## Qualification and hotload

Conversation descriptor revision is **1.0.2**, up from 1.0.1; `module.conf` remains `id=conversation`. The existing Core hotload system requires a changed binary hash and a strictly increased version and performs candidate qualification and authorization.

Build with `make`. The `make test` target currently compiles only. Descriptor qualification includes synthetic positive and negative source-diagnostic checks plus Interpretation dispatch tests. Real integration, false-positive review, and operator acceptance have **not** been performed in this session; qualification cannot substitute for those tests.
