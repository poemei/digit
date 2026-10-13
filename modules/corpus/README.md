# Corpus module

Read-only knowledge retrieval from the established `/opt/digit/corpus/corpus.tsv`. The record schema is `id<TAB>category<TAB>source<TAB>text`, using original identifiers and source attribution. The module registers the `corpus` command and supports `corpus <term>` with deterministic case-insensitive lexical token matching, without editing existing records. A missing or unreadable Corpus is explicitly reported, never replaced by generated knowledge. The TSV parser source is also compiled into Expressions; the runtime modules do not call one another. Build with `make -C modules/corpus`, install with `sudo make -C modules/corpus install`. No LLM is used.

Qualification integrity: the module executes 13 isolated deterministic retrieval checks against a temporary TSV fixture, including token boundaries, source fidelity, missing records, malformed terms, and inaccessible paths. Reported counts derive from executed checks, not declared PASS fixtures. The production Corpus remains untouched. Qualification requires writable temporary storage at `/tmp`.

Module-local verification (from this module directory): `make clean && make && make test`. The test binary is built under this module's `bin/` and reports its own assertion totals. No repository-root build or test command is required.

## ABI lookup service (0.1.2)

The module also publishes `corpus.lookup`, with request/response structures defined in `include/corpus_lookup.h`, for read-only consumers such as Ask. Lookup continues to use the existing deterministic `digit_corpus_find()` and `/opt/digit/corpus/corpus.tsv`, preserving record ID, source, and exact text. It does not rewrite or infer records. Its existing `corpus` command remains available.
