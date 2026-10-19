# Digit Conversation — ABI 1.5 (0.1.0)

This module registers `digit.conversation` for the existing Core-owned
`digit.message` dispatch. The module receives the unchanged request structure
and looks for a matching SQLite3 key using bounded contiguous sequences of
literal tokens, trying the longest sequence first. This is **read-only key
retrieval**, not a language model, semantic search, or generative conversation.

There is **no hardcoded initial word vocabulary**. A request such as
`explain STN-Labz` may match an existing key `STN-Labz` if that is the
actual key in SQLite3's `digit_kv` table. If the record lives under a
different key, or stores non-displayable binary data, this module cannot
retrieve or render it. No data is fabricated or inserted.

## Authorization

Access is restricted to an authenticated **private IRC DM** from the exact
account configured in `/opt/digit/state/auth/conversation.env`. The file's
first line must contain the verified IRC account name alone (e.g. the actual
account-tag, not a nickname); there are no embedded usernames or passwords in
the source. If the configuration is absent/invalid, all requests are denied.
Provision the file with operator-only permissions, outside the repository.
The module never serves public-channel requests.

## Architecture

Core alone decides when to forward a request to `digit.conversation`.
The module invokes `digit.sqlite3.get` through the host ABI 1.5 callback.
Core's service registry must release its lock while invoking services, and
must protect in-flight handlers during unregistration. No direct SQLite
connection, database schema edits, Core mutation, or nested DSD service calls.

The existing DSD observer will report the outer `digit.message` result.
For an internal database trace, explicitly propagated trace context and
SQLite3 instrumentation are still needed; this module does not fabricate
database spans.

Build/qualify: `make -C modules/conversation clean all test`.
Activation/deployment and end-to-end testing are operator-controlled.
