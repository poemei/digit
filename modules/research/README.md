# Digit Live Research — native module v1.0.0

Provides a Core-mediated `digit.research.search.v1` read-only research service and `/research <question>` command. No Core, Router, IRC, Ollama, or DSD module changes.

## Operation

Search is performed only against the fixed Brave Search HTTPS API host with certificate and hostname validation, redirects prohibited, proxies disabled, 10-second request timeout, 64 KiB body limit, at most five returned source links, strict provider SafeSearch, and a shared 12-requests-per-minute budget. Untrusted source excerpts are returned as evidence, not instructions or verified facts. No arbitrary sites are fetched or programs downloaded.

The service request and response ABI are declared in `include/research.h`. Request: `research_request_t.query` (NUL-terminated, at most 255 bytes). Response: `research_response_t`, containing `status`, `sources`, `retrieved_utc`, and bounded `evidence`. Status is EVIDENCE, UNAVAILABLE, or DENIED. No results or credential failures explicitly fail closed instead of fabricating an answer.

Safety: local intent-denial rules reject specified high-risk actionable requests; they **are not a comprehensive legal or content classification engine**. Defensive research remains allowed. Operator review and better policy classification are required before broad untrusted public access. Source excerpts may still be false, manipulated or harmful; consumers must treat them as untrusted data. Do not let source text dictate Digit instructions.

## Credential provisioning (operator only)

Obtain a Brave Search API key from its vendor, then create `/opt/digit/auth/research/brave.key` as a single line with mode 0600, owned by the Digit service user or root. Do not commit keys, place them in `module.conf`, or reveal them in IRC or logs. Without the key the module reports live research unavailable; it never substitutes Ollama memory.

## Qualification and deployment

```sh
make -C modules/research clean all test
```

Static module qualification executes ten positive checks and a negative malformed-response check. Build and tests have not yet been executed against this GitHub revision. Installation, hotload, activation, credential configuration and operator client acceptance have **not** been performed.

Operator check after authorized installation: `/research when is the next SpaceX launch?`; expect dated search excerpts with HTTPS source links, **not** a confident schedule generated from old model knowledge. Negative checks: unsafe actionable prompt should be denied; with absent key, research should be unavailable, not invented; exceeding budget should produce a rate limit.

**Integration boundary:** Ordinary natural-language conversation still needs its owning conversational module to invoke `digit.research.search.v1` and validate those sources; this module does not change the DSD, Core, or Ollama routing and does not claim to repair free-form questions by itself.

**State: source implemented, builds/tests and operator qualification pending.**
