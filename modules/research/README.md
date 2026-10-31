# Digit Research — 1.5.7 (Unreleased)

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
  as fit their existing IRC reply boundaries (349 and 439 bytes respectively),
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

## 1.5.5 — Rebuilt search evidence presentation

The existing one-request DuckDuckGo search path is unchanged: no result-page
fetching, added provider, paid API, search-topic allowlist, or Core change.
The Ask-facing formatter now prioritizes source titles and substantive
result excerpts with complete URLs retained as provenance. Responses carry
source-associated excerpts instead of a mostly undifferentiated inventory
of links, preserving provider order, the existing safety gate, rate limit,
and research service ABI. The command-facing compact result view is retained.

A regression fixture asserts that a factual event time in an HTML result
snippet survives parsing and formatted evidence. This tests information
handoff, not the factual reliability of live search results or the inference
provider. If a single search returns no useful factual snippet, Research
cannot establish the missing fact without another source. Native compilation,
qualification, activation and operator end-to-end acceptance are outstanding.

## 1.5.6 — KISS direct IRC results

The conversational Research service now formats its single DuckDuckGo result set within the IRC-sized 439-character output boundary. Results are returned as source titles, excerpts and complete URLs, not an inferred or synthesized answer. The explicit `/research` command remains a short search-results view. Service names, result status, safety, rate controls, exact-query encoding, single acquisition and HTTPS protections are unchanged. This version is intended for direct delivery by Ask without invoking language inference. Tests and live deployment require operator qualification.

## 1.5.7 — Nearest upcoming result

For explicitly upcoming WHEN questions (next/again/upcoming), Research extracts the subject from the request and includes the current UTC date and generic schedule/date/time/location terms in its one DuckDuckGo search. Other queries remain unmodified. The result parser excludes DuckDuckGo's advertising disclosure link. For upcoming queries, the IRC reply selects the nearest explicitly future-dated result from titles and snippets, preserving its source URL and accompanying details; ordinary search queries continue returning a concise source list. No sports/brand-specific rules, additional network requests, inference or page crawling. Missing facts are not guessed. Source-level change only; native build, tests and live acceptance remain unverified.
