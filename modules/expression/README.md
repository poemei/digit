# Corpus-backed Expressions

Version 0.2.0. Frozen ABI 1.5. Registers the generic `express` command on activation and unregisters it on stop. No expression, vocabulary, or answer is hardcoded.

The expression command reads `/opt/digit/corpus/corpus.tsv` via the shared read-only TSV parser in `modules/corpus/src/corpus_store.c`. Schema: `record_id<TAB>category<TAB>source<TAB>text`. Source attribution comes from matching records; an absent term returns an unknown expression. Corpus is never created, rewritten or truncated by either module.

Invoke `express <term>` in Digit's console. A bare term is not a registered command; it is not silently redirected into a capability module. The full four-field corpus data remains on disk. The lookup is deterministic lexical retrieval, not a language model or inference engine.

Build: `make -C modules/expression`. Install: `sudo make -C modules/expression install`. Corpus file must be readable by the runtime user.

Qualification integrity: the module executes 13 isolated deterministic retrieval checks against a temporary TSV fixture, including token boundaries, source fidelity, missing records, malformed terms, and inaccessible paths. Reported counts derive from executed checks, not declared PASS fixtures. The production Corpus remains untouched. Qualification requires writable temporary storage at `/tmp`.
