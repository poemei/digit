# Digit Research — 1.5.1 (Unreleased)

Research searches the supplied text once and returns source results. It does
not answer the question, rewrite the query, add search directives, fetch the
result pages, specialize by subject, or claim that retrieved content is true.

## Request path

- `/research <search terms>` and `digit.research.search.v1` explicitly search.
- `digit.research.answer.v1` retains the existing conversational claim gate
  used by Ask, then uses the same search implementation.
- The engine URL-encodes the exact supplied text for the existing DuckDuckGo
  Lite HTTPS endpoint. Case, punctuation, operators and word order survive.
- Up to six distinct HTTPS results retain provider order, titles, URLs and
  snippets. Missing dates, short terms, synonyms and title-only results do
  not cause rejection. There is no secondary keyword relevance gate.
- The search service returns the results within its existing 8192-byte field.
  `/research` and the Ask-facing service return as many complete result URLs
  as fit their existing short reply boundary (349 and 439 bytes respectively),
  bounding titles/snippets around those URLs. `sources` counts displayed
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
