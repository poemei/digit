# Digit Developer Self-Diagnostics (DSD) 0.4.0

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

This version supplies the diagnosis engine and contract, with a bounded
request-entry observation from IRC; Core and SQLite3 internals are not instrumented. The source
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

**Integration status:** IRC 0.2.14 routes `show dsd` and
`diagnose my last request` to DSD only from authenticated private messages.
The IRC adapter records a bounded previous request per IRC account. It
still cannot trace through the implementation of `digit.message` inside Core;
Core and ABI are unchanged. Trace-ID and arbitrary-module commands are not
exposed by the IRC adapter because cross-account trace authorization is not
implemented.

**Security:** The command service has no independent authenticated actor
context. A future authorized entry adapter must enforce DM scope and trace
access before calling it. Do not expose this service to untrusted input
without such a check. This module retains no per-user message text.

## Silent request boundary

When IRC records `NO_RESPONSE` from `digit.message`, DSD now reports
that the request reached the service boundary and returned no reply. It
identifies the last instrumented IRC source location, but **does not** claim
to have traced execution within Core. Regression qualifications cover this
case. Further internal drill-down requires separate observed module events
through the existing ABI; Core and its governance remain untouched.

## ABI disposition evidence (0.4.0)

IRC 0.2.14 records the actual returned ABI disposition rather than labelling
all non-success returns as an unavailable service. DSD reports the evidence
in plain language: a missing service, a rejected argument, unauthorized or
invalid-state requests, module qualification/quarantine status, or a successful
service invocation with no reply. It does **not** claim these results prove
where the defect originated, or that a missing conversational response is a
Core fault. These observations identify only the boundary the IRC adapter
was able to measure. Separate instrumentation is needed for database lookup,
record selection, or downstream module results.

The operator must qualify and activate updated modules; source commits are
not evidence of successful deployment or runtime acceptance.

## On-demand SQLite3 probe (SQLite3 0.1.3 / IRC 0.2.15)

When the authenticated private IRC client requests `diagnose my last request`,
IRC reports DSD's stored request-boundary observations and, as a **separate**
private message, calls `digit.sqlite3.diagnostic`. The SQLite3 module checks
its own live database handle and the presence of its `digit_kv` table using
a read-only prepared statement. Its response includes status, the inspected
stage, a sanitized explanation and the module source location. It never
queries record contents, modifies rows, runs migrations, or changes Core.

**Important:** The probe is a current SQLite3 health observation, not a
historical child span of the user's prior request. In this rebuild,
`digit.message` does not dispatch ordinary `define php` queries to a
conversational/SQLite3 pipeline. DSD must report that SQLite3 was not
observed on that request. The separate probe cannot establish whether the
missing pipeline is a defect. The actual request-to-database trace requires
instrumented downstream modules and an existing approved path executing
those modules; do not claim an end-to-end trace from a synthetic probe.

The DM does not accept public-channel DSD commands. Source edits are not
deployment or qualification evidence. Operator end-to-end acceptance is pending.
