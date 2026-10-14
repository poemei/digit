# Digit Changelog

This log distinguishes committed source, operator-observed runtime behavior, and explicit operator acceptance. Historical development records in the roadmap remain evidence of earlier stages; they do not override the following accepted milestones.

## 2026-10-13 — Accepted operator demonstrations

### Interpretation 0.1.0 and Ask 0.1.5 — ACCEPTED
- Independent `interpretation.classify` service classifies bounded capability and knowledge-question intent, and extracts knowledge subjects from supported conversational forms.
- Ask consumes the classifier via ABI 1.5 and uses read-only Corpus retrieval to respond with existing text, record ID and source.
- Operator demonstrated over IRC: `what can you tell me?`, `what are your capabilities?`, `tell me about the Chaos Foundation`, `describe Sentinel`, and `describe the Chaos MVC`.
- Explicit acceptance followed the demonstrated responses. This is **bounded lexical interpretation**, not unrestricted natural-language reasoning. Unrecognized-request behavior was not established by the acceptance transcript.

### Ask 0.1.4 and Corpus 0.1.2 — ACCEPTED
- Added read-only `corpus.lookup` ABI service and `what is <subject>?` handling in Ask.
- Operator demonstrated a Corpus-backed response to `what is the Chaos Foundation?` over IRC, including record identifier and `interface:learn` provenance; explicitly accepted.
- Production `/opt/digit/corpus/corpus.tsv` was not changed by these source revisions.

### IRC 0.1.1 — ACCEPTED
- Per-sender/target previous-request tracking, bounded history, and multiline DSD reports through separate IRC messages.
- Operator demonstrated `help` followed by `diagnose my last request` from `STN_Boss`, then repeated as `PoeMei`, with distinct reported request identifiers. Explicitly accepted.
- Accepted transport behavior does not claim resistance to nickname impersonation.

### Corpus 0.1.3 — OPERATOR ACCEPTED (2026-10-13)
- Introduced native-console `learn CATEGORY | SOURCE | TEXT` for append-only `corpus.tsv` records; not exposed as a public write service.
- Public IRC/Windows `/ask` does not provide the CLI command. Source checks `local-console`/ `local` metadata; this is not independent operator authentication.
- The operator explicitly accepted Corpus 0.1.3 on 2026-10-13. Specific CLI append/readback and negative-interface test logs were not furnished here; acceptance is recorded without inventing test evidence.

## Record of evidence
Operator-provided IRC transcripts constitute observed client behavior for the stated scenarios, not a full automated test-suite report or a claim about every possible input. These notes do not authorize new development.
