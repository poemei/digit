# Digit Research — native module v1.4.8

Research is a **general-purpose, subject-independent information acquisition module**. Search terms come from the user's question or learning directive, not from a programmed subject list.

## Processing flow

1. The existing `digit.research.answer.v1` service claims factual, current-information, investigation and learning requests. Ordinary conversation remains unclaimed.
2. Research extracts significant terms from the question. It retains subjects such as `New England Patriots`, `PHP`, `Native C`, `C#`, `C++`, `Go`, `Rust`, and anything else encountered. **These examples are not a subject allowlist.**
3. It queries an keyless HTTPS web-search page. It rejects results that do not match the actual subject, deduplicates leads, and makes up to three searches with generic schedule, learning or explanatory refinements when initial coverage is sparse.
4. It fetches up to two HTTPS source pages discovered through search, extracts readable text without executing scripts, and considers that material when the search excerpt lacks a needed date.
5. Research returns a source-attributed excerpt or, if facts are incomplete, an explicitly identified source lead. Time-oriented questions distinguish source-reported dates from results lacking a date. Where a date cannot be established, Research returns the relevant lead explicitly marked as unconfirmed rather than suppressing it. **No module-to-module-to-module response chain is required.**
6. The final `/chat` or `/research` reply contains a bounded, source-attributed excerpt or an explicitly unconfirmed lead, including its source URL when it fits the transport reply.

The existing structured SpaceX launch provider remains a successful specialized **optimization**, with the same general search fallback when it does not produce a result. This is not a SpaceX-only system.

## Contracts and boundaries

- `/research`, `digit.research.search.v1` and `digit.research.answer.v1` remain registered. Core, shared ABI, IRC, Ask and Ollama code are unchanged.
- No paid API key, credentials, new provider, or additional general-purpose module is required.
- Read-only HTTPS; TLS verification, socket-level checks against private/reserved destinations, no redirects or proxies, bounded response sizes, per-call timeouts and an existing 12-per-minute research gate.
- Module-owned diagnostic events append to Digit's existing `/opt/digit/logs/digit.log`. At the operator's request, they include exact search queries, constructed URLs and bounded result titles/excerpts; access to that log should remain restricted.
- No execution of retrieved instructions or code. The limited harmful-intent gate remains in Research.
- Retrieval, extraction and answer selection work independently of Ollama. Ollama does **not** own research, choose sources, classify research intent or become a dependency of the Research module.
- **Limits:** The external HTML search service may change its markup, rate-limit automated requests, become unavailable, or return stale and incomplete results. Relevance and date checks cannot fully establish truth, source authority, schedule freshness, semantic contradictions or comprehensive factual answers. A short source excerpt may lack the complete answer; in that case Research should decline rather than invent one. This is a functioning general *search and reading workflow*, **not** full ChatGPT-grade research reasoning or an autonomous learning/knowledge-storage system.
- Learning directions retrieve evidence; they **do not yet persist knowledge or establish demonstrated competence**. Retention and validation are separate authorized work.

## Source and qualification

- `src/research.c`: controller, safety gate, Core-mediated service and explicit command.
- `src/research_engine.c`: term extraction, keyless search, result relevance, bounded page reading and grounded answer extraction.
- `src/research_engine.h`: independent engine contract.
- `tests/test_research.c`: fixture-based tests for unrelated result rejection, Patriots schedule facts, PHP documentation, Go, Native C, C#, HTML handling, missing dates, and retained SpaceX behavior.

Operator-controlled build gate:

```sh
git pull
make -C modules/research clean all test
sudo make -C modules/research install
```

**Status:** Source and module version updated to 1.4.8; native build and retest after this version correction, installed activation, live retrieval, and operator acceptance remain unverified.

Research removes leading conversational addressees from interrogative queries and supports `when are` schedule phrasing. Unavailable evidence distinguishes provider retrieval failure, zero relevant results, and insufficient factual detail. Live success remains unverified.

### Search construction diagnostics (v1.4.6)

For each general search attempt, Research appends `RESEARCH SEARCH_QUERY terms=<constructed terms>` and `RESEARCH SEARCH_REQUEST url=<exact HTTPS URL>` to `/opt/digit/logs/digit.log` before the network request. If URL encoding or length prevents construction, `RESEARCH SEARCH_BUILD outcome=...` records the failure. These entries intentionally expose the actual search text to the operator; access to Digit's log must remain restricted. This instrumentation does not assert that the requested URL returned useful results.

### Evidence-preserving result handling (v1.4.7)

When the search returns relevant results but no dated statement suitable for a scheduling answer, Research returns a clearly identified source lead rather than an empty result. Source statements are attributed, and uncertainty is not concealed. The Research log records each considered hit with its title, excerpt, URL, and whether a recognized date appeared. Schedule query variations no longer repeat `schedule`. The original host-level binary compilation, full test suite, live source-date interpretation, and operator acceptance remain unverified.

### Search snippet and large-page repair (v1.4.8)

Research now extracts nested-markup search snippets through the closing result element, rather than stopping at the first bold or emphasis tag. The bounded source fetch limit increases from 512 KiB to 4 MiB, while the extracted text cap increases to 96 KiB. HTTP failures record the active byte cap. These are bounded acquisition repairs; the operator's Patriots example demonstrated six accepted hits with damaged snippets and a 508403-byte HTTP write failure. Native test execution and live retrieval acceptance are still pending.
