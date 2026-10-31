# Digit Research — 1.5.4 (Unreleased)

Research searches the supplied text once and returns source results. It does
not answer the question, rewrite the query, add search directives, fetch the
result pages, specialize by subject, or claim that retrieved content is true.

## Request path

- `/research <search terms>` and `digit.research.search.v1` explicitly search.
- `digit.research.answer.v1` searches any nonempty permitted conversational query,
  without a keyword or subject allowlist. The existing safety and rate gates remain.
- The engine URL-encodes the exact supplied text for the existing DuckDuckGo
  Lite HTTPS endpoint. Case, punctuation, operators and word order survive.
- Up to six distinct HTTPS results retain provider order, titles, URLs and
  snippets. Missing dates, short terms, synonyms and title-only results do
  not cause rejection. There is no secondary keyword relevance gate.
- The search service returns the results within its existing 8192-byte field.
  `/research` and the Ask-facing service return as many complete result URLs
  as fit their existing short reply boundary (349 and 1699 bytes respectively),
  budgeting a bounded share of space per result so a lengthy first snippet cannot hide later complete URLs. `sources` counts displayed
  results. Long URLs that cannot fit are reported as a reply-size limitation.
- `RESEARCH_EVIDENCE` preserves the existing ABI value and means retrieved
  search results, not independently verified facts or demonstrated learning.

## Existing boundaries

Core-mediated registration, service names and request/response layouts remain
unchanged. The existing safety and 12-requests-per-minute gates remain.
HTTPS certificate verification, destination socket checks, no redirects or
proxies, the 4 MiB response cap and eight-second request timeout remain.
The existing Digit log records exact search text and encoded request URLs.

Provider failures, challenges and unrecognized result markup are distinguished
from an explicit provider no-results response. Provider availability and HTML
layout remain external dependencies. No new dependency or paid API is added;
the unused json-c link dependency is removed with the specialized launch path.

## Build and qualification

```sh
make -C modules/research clean all test
```

The test build substitutes only the HTTP acquisition boundary with fixtures.
It exercises the real query encoding, parser, result formatting, services,
command, rate limit, negative cases and lifecycle registration/rollback.
The production shared module uses real libcurl HTTPS. Module qualification
runs deterministic positive and negative checks without network access.

Validation receipts and remaining limitations are recorded in
`docs/MODULE_ROADMAP.md`. Installation, live Digit routing and operator
acceptance are separate from source build and test results.

## Result-budget correction — 1.5.2

The existing 1.5.1 formatter allocated all remaining reply space to each result in sequence, allowing a verbose first snippet to crowd out additional retrieved URLs in the 349/439-byte command/Ask replies. The short formatter now budgets at most 170 bytes per result while preserving whole HTTPS URLs and provider order. Full search service responses use a larger per-result budget. A regression fixture uses a deliberately long first description and verifies that the second complete URL survives in a 350-byte result. No query rewriting, source-page summarization, Core or Ask changes were introduced. Native build/test and live operation are not yet verified.

## 1.5.3 — Evidence for language inference

The Ask-facing service formats up to 1699 bytes of source results so Ask can pass bounded evidence to the existing replaceable language inference provider. Retrieval remains subject-independent, preserving titles, snippets, URLs and DuckDuckGo provider order. No topic-specific answer rules or new search dependency have been introduced. Live factual answer extraction, build qualification, installation and operator acceptance remain unverified.

## 1.5.4 — General-purpose safe research admission

Removed the conversational `research_claims()` keyword list and the corresponding skip path. Any nonempty safe request reaching Research through the existing Ask-mediated service now receives the same DuckDuckGo search attempt regardless of wording or subject. The explicitly prohibited-request safety gate remains. Identity and peer-directed messages are still handled by Ask before Research. No Core, shared ABI, DuckDuckGo parser, or language provider change. This admits routine conversational text as well as questions; rate limits and source-evidence limitations still apply. Source change only; native qualification and live acceptance are pending.
