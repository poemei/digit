# Digit Conversation 0.4.1 — read-only corpus retrieval and diagnostics

Core sends ordinary `digit.message` requests to `digit.conversation`.
Conversation calls the SQLite3 module's `digit.sqlite3.corpus_search` service,
which queries the existing `corpus_fts` index joined to `corpus`.
No direct database connection exists in Conversation.

The operator has explicitly declared every entry of her existing corpus
verified. SQLite3's read-only search service therefore returns a
`operator-verified` display status for corpus hits, irrespective of legacy
`corpus.verification_status` values. The stored column is **not modified**.
Source record IDs and excerpts are retained in replies. The response is
retrieval evidence, not an invented generated explanation.

**No additional configuration files. No INSERT/UPDATE/DELETE, no schema
migrations, no database write services.** SQLite3 opens its database with
`SQLITE_OPEN_READONLY`.

## DSD

IRC binds a per-worker-thread trace ID through `digit.dsd.bind` before
Core dispatch and clears it after the service returns. SQLite3 and Conversation
can query `digit.dsd.current` and record observed search outcome events
through `digit.dsd.record`. DSD displays bounded multi-stage traces for
`show trace` / `diagnose my last request`, including lookup status, counts,
source files and lines where the actual modules emit evidence. No Core or
ABI 1.5 changes are needed.

When DSD is absent, lookup services still work; internal evidence is simply
unavailable. DSD only reports observed events; failed/missing instrumentation
is not presented as a success.

Build and qualify with:

```sh
make -C modules/dsd clean all test
make -C modules/sqlite3 clean all test
make -C modules/conversation clean all test
make -C modules/irc clean all test
```

Deployment and live IRC acceptance are operator-controlled.
