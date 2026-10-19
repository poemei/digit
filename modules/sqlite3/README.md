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

Database: `/opt/digit/state/sqlite3/digit.db`.
The service account must have write access to that directory before activation.
The module creates its own `digit_kv` table using prepared statements and
supports binary values of 0–1024 bytes.

## Operator-controlled setup

```sh
sudo install -d -o digit -g digit -m 0700 /opt/digit/state/sqlite3
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

**Integration limitation:** Digit's current Core host exposes only
`invoke_service`; it does not yet implement `register_service` and
`unregister_service`. This module consequently fails activation closed
until Core provides the ABI service registry. Do not bypass that boundary
or change Core without operator authorization.
