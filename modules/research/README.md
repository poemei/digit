# Digit Research — native module v1.4.0

**Research is topic-independent.** A question or research/learning instruction determines search terms at runtime, not a hardcoded list of technologies, sports teams, companies or languages.

## Flow

1. Research's `digit.research.answer.v1` service decides whether the utterance calls for investigation. Unclaimed ordinary conversation returns `RESEARCH_NOT_APPLICABLE`.
2. For a claimed question, Research extracts meaningful words from the question, retaining the subject and request intent. Examples are `patriots play schedule`, `php official documentation`, `native c official documentation`, `go official documentation`, and `rust official documentation`—**examples, not subject-specific rules**.
3. It searches a public, keyless HTTPS RSS search endpoint. It rejects unrelated entries based on subject terms, consolidates unique results and refines the search when evidence is sparse. Refinements are selected from general learning, scheduling and explanatory intents, not topic names.
4. It follows up to two HTTPS source pages discovered from those results, with TLS verification, no redirects or proxies, a socket-level restriction against private/reserved destinations and bounded response sizes. HTML scripts, styles and navigation are excluded from extracted text.
5. Research assembles a bounded evidence prompt and, when needed, calls the separately registered `digit.llm.generate` language generator **through Core**. Ollama provides language only; it does not classify questions, search, choose sources or own the response. The prompt demands concise evidence-only answers. Generated answers containing unsupported numbers or source URLs are rejected.
6. Research returns the answer via its service, without search logs, URLs or raw article dumps. If available evidence cannot support a factual answer, it says so briefly rather than guessing.

The existing structured SpaceX launch provider remains a narrow optimization **with general search fallback**. It is not a subject limit or a model for adding one hardcoded handler per topic. The existing `digit.research.search.v1` service is preserved. Explicit `/research` also uses the new general workflow.

## Boundaries and important limits

- No pay-as-you-go search APIs, credentials, or subscriptions are required.
- Web results and generated prose are **not inherently verified facts**. The module uses relevance filters, short multi-source excerpts and numeric grounding checks; these cannot guarantee full semantic truth, freshness, or agreement between sources. The public RSS endpoint is unofficial and may have coverage, access or ranking issues. The configured local LLM can still omit supported facts or produce wording errors.
- Its general research flow can retrieve material about PHP, Native C, C#, C++, HTML, CSS, Go, Rust and other subjects. This implementation **does not yet write learned facts into Digit's persistent knowledge**, which is separate future work and remains authorization-gated.
- The module does not autonomously install tools, execute material from pages, or change project code. Source content is untrusted data, not instructions.
- Search rate limit: 12 claimed queries per minute; up to three search attempts and two source-page reads per request. Individual HTTPS calls have a bounded response size and timeout. General answer synthesis requires the existing Core-mediated `digit.llm.generate` service.
- Safety screening remains a limited phrase-based gate, not a comprehensive guarantee against harmful content.

## Build and qualification (operator-controlled)

```sh
git pull
make -C modules/research clean all test
sudo make -C modules/research install
```

The native C module and test target compile `src/research.c` plus `src/research_engine.c` with libcurl, json-c and pthread. Offline tests exercise term generation, subject filtering, RSS/HTML extraction, programming language terminology, result grounding and the previous structured launch path.

**Source committed.** Native compilation, qualifications, live provider retrieval, activation and operator acceptance must be distinguished. No Core, ABI, IRC, Ask or Ollama source was changed as part of this release.
