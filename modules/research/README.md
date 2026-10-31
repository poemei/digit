# Digit Research — native module v1.4.2

Research is a **general-purpose, subject-independent information acquisition module**. Search terms come from the user's question or learning directive, not from a programmed subject list.

## Processing flow

1. The existing `digit.research.answer.v1` service claims factual, current-information, investigation and learning requests. Ordinary conversation remains unclaimed.
2. Research extracts significant terms from the question. It retains subjects such as `New England Patriots`, `PHP`, `Native C`, `C#`, `C++`, `Go`, `Rust`, and anything else encountered. **These examples are not a subject allowlist.**
3. It queries an unofficial, free, keyless HTTPS RSS search endpoint. It rejects results that do not match the actual subject, deduplicates leads, and makes up to three searches with generic schedule, learning or explanatory refinements when initial coverage is sparse.
4. It fetches up to two HTTPS source pages discovered through search, extracts readable text without executing scripts, and considers that material when the search excerpt lacks a needed date.
5. Research itself selects a relevant factual excerpt as a concise answer. Time-oriented questions need explicit date/time evidence; otherwise they receive a brief inability-to-verify response. **No module-to-module-to-module response chain is required.**
6. The final `/chat` or `/research` reply contains the selected information, not URLs, research timestamps, source listings or raw multi-result dumps.

The existing structured SpaceX launch provider remains a successful specialized **optimization**, with the same general search fallback when it does not produce a result. This is not a SpaceX-only system.

## Contracts and boundaries

- `/research`, `digit.research.search.v1` and `digit.research.answer.v1` remain registered. Core, shared ABI, IRC, Ask and Ollama code are unchanged.
- No paid API key, credentials, new provider, or additional general-purpose module is required.
- Read-only HTTPS; TLS verification, socket-level checks against private/reserved destinations, no redirects or proxies, bounded response sizes, per-call timeouts and an existing 12-per-minute research gate.
- Module-owned diagnostic events append to Digit's existing `/opt/digit/logs/digit.log` without storing queries or source content.
- No execution of retrieved instructions or code. The limited harmful-intent gate remains in Research.
- Retrieval, extraction and answer selection work independently of Ollama. Ollama does **not** own research, choose sources, classify research intent or become a dependency of the Research module.
- **Limits:** Bing RSS is an unofficial source and may be sparse, unavailable, stale or incomplete. Relevance and date checks cannot fully establish truth, source authority, schedule freshness, semantic contradictions or comprehensive factual answers. A short source excerpt may lack the complete answer; in that case Research should decline rather than invent one. This is a functioning general *search and reading workflow*, **not** full ChatGPT-grade research reasoning or an autonomous learning/knowledge-storage system.
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

**Status:** Source and module version updated to 1.4.1; native build and retest after this version correction, installed activation, live retrieval, and operator acceptance remain unverified.
