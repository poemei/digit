# Digit Research Module — 1.0.1

Native C module for Digit ABI 1.5. No ST-1 Mission dependency, Slack, IRC changes, or Core changes.

## Correction in 1.0.1

Removed all chat-command registration. Version 1.0.0 incorrectly registered `research` as a command and assumed IRC would route conversational messages through the command registry. That assumption was not supported by runtime evidence.

Research now registers only the standard ABI service `digit.research.lookup`. This does **not** add IRC routing or make Digit's conversational interface invoke research by itself. No such integration is claimed or silently patched.

## Typed ABI request and response

See `include/research_service.h`.

- Input: `digit_research_request_t` with NUL-terminated `topic[64]` and `terms[512]`.
- Output: `digit_research_response_t` with `text[512]` and `verified=0`.
- Callers use the Core service registry's `invoke_service` function; modules must not bind one another directly.
- Valid topics: `security`, `autonomous-systems`, `software-engineering`, `space-systems`.

## Architecture and boundaries

- Controller: `src/research_module.c` provides service entry and lifecycle.
- Model: `src/research_model.c` searches local text evidence.
- View: no IRC-specific or command-specific rendering; response is structured for a separate authorized caller.
- Source root: `/opt/digit/research/sources/<topic>/*.txt`.
- Source excerpts are always UNVERIFIED, never automatically treated as Digit's approved knowledge.
- No automatic online research, scheduling, or source ingestion has been implemented.

## Build and installation

```sh
make -C modules/research clean all test
sudo make -C modules/research install
```

Then inspect Digit's module audit for `module=research` and `version=1.0.1`. Existing hotload/version qualification rules apply.

**Qualification note:** `make test` now includes `test_research_abi`, an isolated mock-host test of service registration, invocation, rejection of invalid requests, and unregistration. This is not a test of Digit's live Core registry, and does not establish conversational integration or autonomous research. The tests still require execution and evidence from the operator.

No operational interaction with Rictus, and no changes to Core or IRC.
