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
