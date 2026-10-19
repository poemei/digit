# Digit S2: defensive intelligence, first milestone

ABI 1.5 module; independent from Core, IRC, Rictus and Sentinel.

Sources (read-only):
- `/var/log/rictus/rictus.log`
- `https://api.stn-labz.com/threats` via TLS, certificate verification enabled.

Service `digit.s2.poll` returns at most one sanitised report for IRC.
The IRC adapter uses a one-second idle socket check to trigger service polls
at most once every 5 minutes, including periods with no IRC messages.
A first-run mission-status report is emitted once source collection completes;
subsequent unchanged snapshots are suppressed. IRC 0.3.1 and S2 0.1.1
must both be installed for this behavior.
This first milestone reads the last 128 Rictus lines, parses bounded Sentinel
JSON, considers the two IP fields separately, and correlates only exact IPv4
tokens appearing in local log lines. Rictus-only observations are **never
dropped**: their new-line count is reported separately as unclassified
pending review; their threat severity is not inferred. Severity and relevance
are kept distinct; only HIGH/HIGH or CRITICAL/HIGH or HIGH/CRITICAL or
CRITICAL/CRITICAL remote findings trigger an incident report on this
milestone. Older remote timestamps are not assumed to use UTC. A matching
Rictus line is corroboration of an indicator, not proof of compromise.

The endpoint may expose additional schema not covered by the supplied sample:
this module supports a top-level array or a `threats` array. Everything
else is reported as unavailable, never interpreted as clear. API freshness
is retained in each record; a newer consistent timestamp is selected for
matching IDs, with provenance retained as a source property.

Read-only and no automated blocks, response, exploitation, database edits or
outbound arbitrary requests. No raw IPs, URLs or log text go to public IRC.
No credentials are embedded in the binary. If the API needs authentication,
deployment must be held until its exact authorized contract is provided.

Build: `cd modules/s2 && make clean && make && make test`.
Install only after tests: `sudo make install`. Install upgraded IRC separately.
Do not restart Core merely to test this module; hotload may admit it.

**Known limitations**: Neither a passive status nor ACTIVE proves the endpoint
returned usable threat data; the operator must test with representative records.
The IRC adapter still performs S2's bounded API fetch synchronously (up to
8 seconds); asynchronous investigation and durable event cursor are not part
of this authorized repair. No new standalone logging sink is introduced.

Additional limitations: Rictus grammar not yet supplied, so its unmatched lines
are not individually classified; a line count is not a definitive attack finding.
TLS read and parsing may fail on an undocumented API response. No persisted
cross-restart cursor, no distributed investigative queries beyond the documented
threats endpoint, no ability to resolve conflicting IP roles or timestamps
without source metadata.
