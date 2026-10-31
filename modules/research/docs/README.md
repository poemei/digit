# Digit Research 1.0.0 (unreleased)

Research is a general-purpose public internet retrieval module. It registers `/research`, `digit.research.answer.v1`, and `digit.research.search.v1` through ABI 1.5. No Core or Ask changes are needed.

The answer service accepts the existing `research_request_t`, searches DuckDuckGo HTML, follows up to three public HTTPS result pages (without redirect following), selects a relevant page passage, and returns up to 438 characters as `research_response_t.evidence`. For explicitly next/upcoming *when* questions, a passage must carry a parseable future date; otherwise it returns `RESEARCH_UNAVAILABLE` rather than guessing. The search service returns titles, excerpts and source URLs within the same existing response contract for source inspection. No LLM is invoked.

Dependencies: libcurl and libxml2 development headers. Build: `make -C modules/research clean all test`. Operator-controlled installation: `sudo make -C modules/research install`. Installation is not authorized by this source package.

Limitations: HTML search layouts can change; pages that require JavaScript or block retrieval cannot be read. Dates must appear in one of the recognized month/day/year or ISO forms. The module cannot establish venue, kickoff time or network if they are absent from a concise retrieved passage. Only public pages with HTTPS links are retrieved. The live provider was unreachable in the development environment, so no live end-to-end acceptance is claimed.
