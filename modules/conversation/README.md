# Digit Conversation 0.3.0 — corpus retrieval

Core dispatches ordinary digit.message requests to digit.conversation.
Conversation uses the ABI 1.5 service digit.sqlite3.corpus_search to query
the **existing** corpus_fts(text) index joined by rowid to corpus(id).

The database contains 48,362 corpus rows and the corpus_fts index also
reports 48,362 entries (operator-supplied observations, 2026-10-19).
No new tables, configuration files, migrations, or database writes.

Requests are split into literal contiguous phrases; up to four terms are
searched, longest phrase first, with no fixed conversational trigger words.
SQLite3 builds a quoted FTS5 phrase, binds it as a SQL parameter, and
returns a bounded set of corpus IDs, verification statuses and excerpts.
The reply shows source evidence, not a fabricated generative explanation.
A no-hit response means no matching phrase was found, not an empty database.

No put/delete service is registered; the database is opened READONLY.
The existing digit.sqlite3.get service remains for backward compatibility.

Offline qualification tests use in-memory fixtures. Full DSD drill-down
into individual retrieval attempts is not yet supported: IRC's current
recording observes the outer digit.message result only.

Operator-controlled verification:
make -C modules/sqlite3 clean all test
make -C modules/conversation clean all test

Live corpus lookup and deployment remain subject to operator acceptance.
