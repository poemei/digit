# Digit Research — native module v1.5.0

Research is a **general-purpose, subject-independent information acquisition module**. Search terms come from the user's question or learning directive, not from a programmed subject list.

## Processing flow

1. The existing `digit.research.answer.v1` service claims factual, current-information, investigation and learning requests. Ordinary conversation remains unclaimed.
2. Research extracts significant terms from the question. It retains subjects such as `New England Patriots`, `PHP`, `Native C`, `C#`, `C++`, `Go`, `Rust`, and anything else encountered. **These examples are not a subject allowlist.**
3. It queries an keyless HTTPS web-search page. It submits the user's question as provided (excluding a leading assistant addressee) and retains only search hits relevant to that question. Research does not add search terms or perform rewritten follow-up searches.
4. It fetches up to two HTTPS source pages discovered through search, extracts readable text without executing scripts, and considers that material when the search excerpt lacks a needed date.
5. Research returns source-attributed material. For a question about the next scheduled game or event, a listing or navigation page does not qualify as an answer: a future-dated event and time must be present in the retrieved evidence, or the answer is reported as unverified. **No module-to-module-to-module response chain is required.**
6. The final `/chat` or `/research` reply contains an attributable excerpt when the requested fact is established. For an unverified upcoming event, Research reports that no next event was established, not an arbitrary directory excerpt.

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

**Status:** Source and module version updated to 1.5.0; native build and retest after this version correction, installed activation, live retrieval, and operator acceptance remain unverified.

Research removes leading conversational addressees from interrogative queries and supports `when are` schedule phrasing. Unavailable evidence distinguishes provider retrieval failure, zero relevant results, and insufficient factual detail. Live success remains unverified.

### Search construction diagnostics (v1.4.6)

For each general search attempt, Research appends `RESEARCH SEARCH_QUERY terms=<constructed terms>` and `RESEARCH SEARCH_REQUEST url=<exact HTTPS URL>` to `/opt/digit/logs/digit.log` before the network request. If URL encoding or length prevents construction, `RESEARCH SEARCH_BUILD outcome=...` records the failure. These entries intentionally expose the actual search text to the operator; access to Digit's log must remain restricted. This instrumentation does not assert that the requested URL returned useful results.

### Evidence-preserving result handling (v1.4.7)

When the search returns relevant results but no dated statement suitable for a scheduling answer, Research returns a clearly identified source lead rather than an empty result. Source statements are attributed, and uncertainty is not concealed. The Research log records each considered hit with its title, excerpt, URL, and whether a recognized date appeared. Schedule query variations no longer repeat `schedule`. The original host-level binary compilation, full test suite, live source-date interpretation, and operator acceptance remain unverified.

### Search snippet and large-page repair (v1.4.8)

Research now extracts nested-markup search snippets through the closing result element, rather than stopping at the first bold or emphasis tag. The bounded source fetch limit increases from 512 KiB to 4 MiB, while the extracted text cap increases to 96 KiB. HTTP failures record the active byte cap. These are bounded acquisition repairs; the operator's Patriots example demonstrated six accepted hits with damaged snippets and a 508403-byte HTTP write failure. Native test execution and live retrieval acceptance are still pending.

### Qualification correction (v1.4.9)

The v1.4.8 nested-highlight regression test mistakenly supplied a shadowing `const char *html` fixture (with `sizeof` equal to pointer size) as the writable destination for `research_choose_fact`. The regression now uses a separate writable 448-byte response array and checks its content. Research's log writer also handles EINTR and short writes instead of discarding `write`'s result. The operator's v1.4.8 build completed; the test reported 112/113 passing with line 205 failing and two `write` warnings. Revised v1.4.9 build/test and runtime acceptance are pending.

### Precision correction (v1.4.10)

Source-page text is now used to supply dated, source-attributed excerpts to Research's answer selector. Previously it was fetched but discarded. Dated schedule rows are recognized without a team allowlist; unrelated subjects continue through generic dated-context matching. Time-sensitive searches continue refining until a dated hit is found (or three attempts are exhausted), and new dated results can replace weaker leads even when all six slots are occupied. Diagnostics report whether each fetched page supplied a dated passage. The source reader remains bounded; structured content hidden in scripts may not be captured. The presence of a date alone does not establish complete chronological accuracy. Build, full regression tests and live operation are not yet verified.

The reader attempts up to four evidence-bearing source pages (rather than always fetching only hit indexes 0 and 1), skipping those with already-dated excerpts. It records which indexes supplied readable content. The structured WEEK-row optimization is invoked only for schedule-like questions, preserving ordinary temporal research behavior for unrelated requests.

### Search authority and schedule-answer precision (v1.5.0)

Research submits the user's original question to the HTTPS search endpoint, preserving words, order, case and punctuation after removing only a leading assistant addressee such as `digicat,`. It does not add `official`, `upcoming schedule`, `date time`, or other search directives. Search remains a single request rather than automatic reformulations. Internal relevance scoring may tokenize the user's question but must not alter the outgoing query. For next-game/event questions, navigation year lists and general schedule directories cannot qualify as an answer. Page extraction looks for future dated events with a nearby kickoff time, and the selector requires a concrete event before claiming research evidence. The source must still be checked in the live operator environment; search rankings, source layouts and page content are external and unpredictable. This update does not imply a verified live result.
