# Digit Live Research — native module v1.1.0

**No subscription, payment account, or API key is required.**

The module provides a Core-mediated `digit.research.search.v1` read-only research service and a `/research <question>` command.

## Operation

Research uses a fixed HTTPS public Bing RSS search endpoint with no credentials. It reads at most 64 KiB, accepts up to five cited headlines per search, forbids redirects and proxy use, validates HTTPS links, limits requests to 12 per minute, and audits a query hash and outcome without logging queries.

**This is an unofficial public RSS interface.** Availability, provider terms, search coverage, and response format are not guaranteed; treat results as unverified search leads rather than authenticated facts. Some questions require primary-source verification beyond this implementation. If the feed is unavailable or inconsistent, Digit must say it cannot verify the answer; it must not use old Ollama knowledge as a substitute.

The deterministic intent-denial gate is a limited safeguard, **not** a complete content moderation policy. Do not enable untrusted public querying without additional review. This module fetches only the hardcoded provider endpoint, not arbitrary requested pages, and cannot download executables. Source content is untrusted data, not instructions.

## Build, qualification, and integration

```sh
make -C modules/research clean all test
```

No credentials need to be provisioned. Build and live runtime results are not yet verified. The operator controls installation and activation.

**Still separate:** Free-form requests such as “Digicat, when is the next SpaceX launch?” need the existing conversational/DSD module to call `digit.research.search.v1` and evaluate source freshness. This change does not modify protected Core or the conversational module.

**Status:** Source changed to keyless research, pending compile, validation, operator acceptance.
