# Conversation v1.0.0

This implementation does not contain conversational keyword triggers, prewritten chat responses, a word-based topic selector, or a rule requiring a corpus match before speaking.

## Inputs
- Live IRC message via `digit.conversation.listen` or `digit.conversation`
- Recent observations via `digit.sqlite3.observation_recent`
- Corpus evidence via `digit.sqlite3.corpus_search`

## Language generation
Requires an **already running local Ollama-compatible inference service** at `127.0.0.1:11434`, and `DIGIT_CONVERSATION_MODEL` set in Digit's process environment to the name of an installed model. No remote/cloud request is made.

The inference engine proposes a corpus lookup based on the conversation, then produces a fresh IRC utterance from the live context, observations and any available corpus evidence. Missing corpus evidence does **not** gate speech.

The module returns a non-OK service result if inference is unavailable. It does not synthesize a fallback or claim conversational intelligence without a working model. Current C transport reads a normal HTTP/1.1 JSON response, not chunked transfer encoding.

## Installation
```sh
make -C modules/conversation clean all test
sudo make -C modules/conversation install
```

Core module activation and live IRC transport must still be checked by the operator. No installation or runtime qualification is claimed by this repository commit.
