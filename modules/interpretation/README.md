# Digit Interpretation — initial lexical capability (v0.1.1)

Native-C ABI 1.5 independent module. Registers `digit.interpretation`.

This initial capability extracts bounded lexical terms, surface question/statement classification, and indicators for negation, time references, reference words, contrast, and causal vocabulary. It does **not** implement semantic comprehension, pronoun resolution, conversation participation, reasoning, or natural-language generation.

No Ollama, LLM, neural inference, network service, database access, or scripted conversational answers are used. Linguistic indicators are not factual conclusions. Unresolved references are reported, not guessed.

## Installation and hotload

Build with `make` in this directory. `make test` currently only builds and does **not** execute tests. The descriptor exposes 24 offline lexical checks, which Core executes as part of module qualification; their success has not been independently verified here.

`module.conf` must contain exactly `id=interpretation` on one line to match Core's existing `discover_new` implementation. Installed layout: `/opt/digit/modules/interpretation/module.conf` and `/opt/digit/modules/interpretation/bin/interpretation.so`.

Digit's running Core polls installed module directories and can discover, verify, qualify, authorize and activate a newly installed module without restart, subject to policy. For an already active replacement, hotload requires a changed binary SHA-256 and an increased descriptor version. The revision was bumped from 0.1.0 to **0.1.1** for that rule. No changes to Core or its lifecycle are required.

This module has not been built, hotloaded or operator-accepted in this session. It is not yet integrated with Conversation and must not be represented as completed conversational intelligence. Installation or activation remains an operator decision.
