# Digit Developer Self-Diagnostics (DSD) 0.3.0

An independent ABI 1.5 module providing bounded, read-only, evidence-led
trace correlation. **Source candidate; operator acceptance outstanding.**

Services:
- digit.dsd.record: accepts a sanitized digit_dsd_record_t event.
- digit.dsd.diagnose: accepts digit_dsd_query_t, returns digit_dsd_result_t.

Trace IDs are assigned by the requesting entry adapter (IRC, future /ask,
or an instrumented module); DSD never guesses a trace origin. Each event
identifies module, method, source path and line, operation, status, elapsed
microseconds and sanitized record counts. DSD returns the earliest reported
failing boundary and the complete bounded trail, distinguishing observation
from root cause. No unsolicited repair or SQL changes.

Security: only fixed restricted code characters are admitted for module,
method, status, file and operation identifiers. Never submit tokens,
credentials, SQL text, user text, database values or network payloads.
No user identity or access decision is inferred from possession of trace ID.
The upstream authenticated private DM adapter must enforce authorization.

Limits: 64 trace slots, 32 events per trace; bounded memory, no disk writes.
Overflow reports dropped events; no fabricated source line, inferred SQL
failure, or internal Core trace. Existing Core ABI service registry permits
these services without Core changes. **Consumers must not invoke
digit.dsd.record inside another ABI service handler** because the current
Digit Core holds the service-registry lock during a callback.

This version supplies the diagnosis engine and contract, not instrumentation
of Core, SQLite3, request-entry adapters, or a private-DM command. The source
line is trustworthy only when the caller instruments the real execution site
and records its matching build revision. No data is produced until such a
caller submits genuine events. This is not full end-to-end tracing.

Build: make -C modules/dsd clean all test
Deploy and acceptance controlled by the operator.

## On-demand commands (service-level only)

The module also registers `digit.dsd.command` with a fixed-size request
and response contract defined in `dsd.h`. Commands understood:

- `show dsd`: report the DSD capabilities and limitations.
- `diagnose my last request`: inspect the caller-provided previous trace ID;
  no global last-request state is used.
- `diagnose trace <id>`: inspect a trace if the authorized caller permits it.
- `diagnose module <id>`: inspect the latest recorded trace that contains
  an event for the specified module; **no events** is not evidence of health.

Diagnostic summaries translate recognized status identifiers into readable
descriptions (e.g., SQLITE_BUSY means database is locked). Raw numeric error
codes are not included in conversational summaries.

**Integration status:** The current Core's `digit.message` handler recognizes
only Core module-management commands, not DSD commands. It does not delegate
`show dsd` or `diagnose` to this module, and its module host does not
supply register_command. Consequently those commands are **not reachable
from IRC today**. This is a deliberate architecture limit; no Core, ABI,
IRC or SQLite3 modifications are authorized or included here.

**Security:** The command service has no independent authenticated actor
context. A future authorized entry adapter must enforce DM scope and trace
access before calling it. Do not expose this service to untrusted input
without such a check. This module retains no per-user message text.
