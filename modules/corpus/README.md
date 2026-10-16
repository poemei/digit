# Corpus module

Read-only knowledge retrieval from the established `/opt/digit/corpus/corpus.tsv`. The record schema is `id<TAB>category<TAB>source<TAB>text`, using original identifiers and source attribution. The module registers the `corpus` command and supports `corpus <term>` with deterministic case-insensitive lexical token matching, without editing existing records. A missing or unreadable Corpus is explicitly reported, never replaced by generated knowledge. The TSV parser source is also compiled into Expressions; the runtime modules do not call one another. Build with `make -C modules/corpus`, install with `sudo make -C modules/corpus install`. No LLM is used.

Qualification integrity: the module executes 13 isolated deterministic retrieval checks against a temporary TSV fixture, including token boundaries, source fidelity, missing records, malformed terms, and inaccessible paths. Reported counts derive from executed checks, not declared PASS fixtures. The production Corpus remains untouched. Qualification requires writable temporary storage at `/tmp`.

Module-local verification (from this module directory): `make clean && make && make test`. The test binary is built under this module's `bin/` and reports its own assertion totals. No repository-root build or test command is required.

## ABI lookup service (0.1.2)

The module also publishes `corpus.lookup`, with request/response structures defined in `include/corpus_lookup.h`, for read-only consumers such as Ask. Lookup continues to use the existing deterministic `digit_corpus_find()` and `/opt/digit/corpus/corpus.tsv`, preserving record ID, source, and exact text. It does not rewrite or infer records. Its existing `corpus` command remains available.

## CLI-only learn (0.1.3)

The independent Corpus module registers `learn` as a command, not a network or ABI write service. From Digit's native console only, use `learn CATEGORY | SOURCE | TEXT`. The command requires the console sender and account, validates bounded TSV fields, generates a random unique-style record identifier, opens the existing `/opt/digit/corpus/corpus.tsv` with `O_APPEND|O_NOFOLLOW`, locks it, appends a single record, and synchronizes the file. It never creates or replaces the production Corpus. If the file is absent or write access fails, learning fails closed. Anonymous IRC, Windows GUI, and `/ask` can reach only read-only Ask/Corpus retrieval and cannot invoke this command through the public Router boundary. Keep filesystem write permission restricted to Digit's intended runtime identity. A successful `learn` reports the appended record ID.

Do not confuse `sender=local-console` metadata with cryptographic operator authentication: the current native-console trust boundary is local process access. Module activation and runtime acceptance remain subject to AGENTS.md.


## Local batch ingestion (operator-side)

Build with `make -C modules/corpus import-tool`. The local executable
`modules/corpus/bin/corpus-import FILE` accepts UTF-8 text files, either:
- one existing `digit learn CATEGORY | SOURCE | TEXT` instruction per line, or
- one independent fact per nonblank line, attributed to the input filename under category `DOCUMENT`.

It validates the entire input (8 MiB maximum) before touching Corpus, appends under an exclusive file lock, and rolls back a partial batch on failure. Records have independent random IDs and preserve source attribution. Blank lines and lines starting with `#` are ignored. This is a local host tool, **not** a Slack, IRC, or public import endpoint. It requires OS write permission on the existing append-only Corpus file; invoke under a properly authorized system identity. The input must already be approved for learning; ingestion is not evidence validation. Existing records are never overwritten or deleted.

Example, after building and installing:

```sh
sudo -u digit /opt/digit/modules/corpus/bin/corpus-import /path/readable-by-digit/20261014.txt
```

The source file must be readable by the `digit` user. Review the import count; do not re-import the same file without intending additional append-only records. The utility does not alter Observation.
