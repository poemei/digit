# Digit SQLite3 persistence module

Standalone C ABI 1.5 module, independently built and qualified.
SQLite3 owns only its persistence boundary; it does not modify Digit Core,
identity, routing, IRC transport, or the root Makefile.

## Contract

The module registers three ABI services with Core's `register_service` host function:
`digit.sqlite3.put`, `digit.sqlite3.get`, and `digit.sqlite3.delete`.
The fixed request for put, and response for get, are laid out as:

```c
typedef struct {
    char key[128];
    unsigned char value[1024];
    size_t length;
} sqlite3_record_t;
```

For get and delete, the request is exactly 128 bytes holding a NUL-terminated key.
For get, the response buffer must be at least `sizeof(sqlite3_record_t)`.
Use the same C ABI and word size as Core. No general-purpose raw-SQL service is exposed.

Database: `/opt/digit/data/digit_memory.db`.
The existing database must already exist. Digit needs read/write access to the file and its parent directory for SQLite journaling. The module opens the file without SQLITE_OPEN_CREATE.
The module creates its own `digit_kv` table using prepared statements and
supports binary values of 0–1024 bytes.

## Operator-controlled setup

```sh
sudo ls -l /opt/digit/data/digit_memory.db
cd ~/digit/modules/sqlite3
make clean && make && make test
sudo make install
```

Installing a module does not modify Core's Makefile or restart the systemd service.
Because Digit automatically activates qualified modules, adding a module and
restarting Digit may activate it; operator approval of deployment is required.

The qualification suite is deterministic, uses `:memory:`, and does not
open the persistent database. The module's 26 checks cover schema, binary
round-trips, key validation, missing keys and negative/failure behavior.

Core now provides the ABI service registry. Startup may add the module-owned `digit_kv` table to the existing database using `CREATE TABLE IF NOT EXISTS`, but does not drop or replace other tables. Back up the database before deployment.


## Context services (v0.4.5)

The SQLite3 module alone owns access to the existing `context` table. `digit.sqlite3.context_store` records an observed channel utterance only when its Observation source exists, and stores the original Observation event ID in `source_id`. `digit.sqlite3.context_recent` reads up to eight non-expired context rows for the known IRC channel. The stored classification records *utterance form*, not truth or inferred intent.

No database migration or new database is introduced. Context has no independent natural-language comprehension. SQLite3 descriptor revision is 0.4.5, following the version-numbering doctrine. Qualification includes in-memory SQL insertion, provenance and retrieval checks; live database behavior and actual module hotload require operator acceptance.


## Atomic Observation context (v0.4.6)

SQLite3 now creates an `utterance.observed` context record within the same transaction as each successful `digit.sqlite3.observation_insert` call. Context `source_id` is the exact new Observation `event_id`; `subject_id` is the actual Conversation ID. Failure to insert either record rolls back both. No schema migration, Core edit, new database, or change to the Observation request ABI. Existing context services remain available for retrieval and compatibility, but the Observation writer owns immediate context persistence. The in-memory qualification exercises the atomic path. Live reload and runtime behavior require operator acceptance.


## Qualification fixture correction (v0.4.7)

The in-memory qualification extends its observations fixture with `sender_id` and `timestamp` before exercising atomic persistence. A subsequent fixture insert now specifies column names explicitly rather than relying on the old five-column shape. This preserves all actual qualification checks and does not change the production schema or runtime SQL behavior. Operator build and test results remain the acceptance authority.


## Transaction diagnosis (v0.4.8)

The atomic Observation writer now emits result-only syslog entries under `digit.sqlite3 observation_atomic`: `status=committed`, `status=rejected phase=... sqlite_code=...`, or `status=rollback phase=... sqlite_code=...`. No message text, sender, account, or SQL row contents are logged. Diagnostics do not replace end-to-end acceptance.


## Verified Observation receipt (v0.4.9)

The existing `digit.sqlite3.observation_insert` ABI accepts an **optional** `digit_observation_receipt_t` response. Callers providing a response buffer receive a completed receipt only after a successful transaction COMMIT. The receipt contains the persisted observation row ID, linked context row ID, and exact event ID; it is populated by verifying the provenance join before commit. Older clients supplying NULL response remain supported. An undersized non-null response is rejected without writing.

Qualification's in-memory writer runs are marked internally so they do not emit production `observation_atomic role=runtime status=committed` messages. Native qualification now checks the receipt, including a malformed response-buffer case. A passing qualification still requires operator live database acceptance.


## Request-bound DSD context read evidence (v0.4.10)

When a live thread-bound DSD trace is present, `digit.sqlite3.context_recent` emits a sanitized SQL result event identifying the actual prepared SELECT boundary, `SQLITE_OK` or a concrete categorized SQLite failure, source line, and result row count. No SQL statement text, row contents or identities are logged. The existing persistence transaction and optional Observation/context receipt are unchanged. If a request does not invoke this SQLite service, no SQL result is invented. Deploy with DSD 0.7.3 and Conversation 1.0.5.


## Corpus provenance fidelity (v0.4.11)

`digit.sqlite3.corpus_search` now copies `corpus.verification_status` from the stored database record into the returned Corpus hit without overriding it. The in-memory qualification verifies both a `verified` record and an `unverified` CSS record retain their actual values. This is a read-only retrieval correction: no schema changes, SQL writes to the runtime Corpus, Core changes, or autonomous verification. Note that Reasoning currently accepts only the literal status `operator-verified`; stored `verified` values are intentionally not promoted by SQLite3 and may remain ineligible until a separately authorized verification-policy decision. Build and live qualification remain operator-controlled.
