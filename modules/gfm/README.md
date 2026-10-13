# GFM 0.1.0

Independent Digit ABI 1.5 GitHub Flavored Markdown structure-analysis module.

Registers the reusable `gfm.analyze` service for other authorized capabilities through the Core-mediated service interface. No Core or Router changes.

The initial bounded, **read-only** service identifies ATX heading lines, list-item candidates, fenced-code blocks, link candidates, table separator candidates, and unclosed fences. Fenced-code content is not treated as prose. It does **not** implement the complete GitHub Flavored Markdown specification or determine whether a document's factual claims are correct. Table and link classifications are heuristics, not parser-certified syntax. No file paths or filesystem writes are accepted by the service.

Development rule: GFM is responsible for Markdown structure, not DSD findings or engineering authorization. Any future edit capability requires a separate explicitly authorized step, exact target and change verification, and protection of Core source.

Build and test: `make clean && make && make test`.

Status: Pending build, qualification, and operator end-to-end acceptance.
