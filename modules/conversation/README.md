# Digit Conversation 0.2.0

Generic request text is passed unchanged through Core's `digit.message`
dispatch to `digit.conversation`. This module reads existing SQLite3
key/value records through `digit.sqlite3.get`, selecting candidates from
request text without hardcoded initial verbs.

**No additional server configuration file is required.** No account allowlist,
new authentication configuration, database writes, schema installation or
migrations are performed by Conversation.

SQLite3 0.2.0 opens the existing database with `SQLITE_OPEN_READONLY` and
exposes only `digit.sqlite3.get` and the read-only diagnostic service.
It does not register put/delete services. This intentionally disables
writes previously offered by the SQLite3 module. The code cannot prevent
an entirely separate process with filesystem write permissions from altering
the database; that requires operating-system permissions.

This is bounded literal key retrieval, not semantic search or generated
conversation. Existing keys must match candidates derived from the request.
Values that cannot safely be sent on IRC are not shown.

Build: `make -C modules/conversation clean all test`.
The operator controls deployment, testing, and acceptance.
