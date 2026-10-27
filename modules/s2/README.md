# Digit S2 — Sentinel Intelligence Module 1.0.1

## Scope

First operational collection milestone for Digit's **Cyber Security** mission: research and engineering of security systems centered on evidence, operator authority, containment and predictable behavior.

- Native Digit ABI 1.5 module; **no changes to Core or the shared ABI**.
- Retrieves the HTTPS Sentinel JSON feed at `https://api.stn-labz.com/threats` every 14,400 seconds (4 hours) on its own worker.
- Bounded JSON ingest (8 MiB / 10,000 records), case IDs, content signatures, new/revised/unchanged counters.
- Existing IDs absent from a later feed are **retained, not automatically closed**.
- Persisted register at `/opt/digit/state/s2/sentinel-register.txt`, written with an atomic rename. Never assume historical bulk import is current activity.
- A registered `/s2` or `/s2 status` command reports the current observed register and last successful collection. No Publisher or Slack dependencies.
- MEDIUM/HIGH/CRITICAL are considered qualifying only when source severity is explicitly present (top-level or `details`). The feed can omit severity; missing is reported, not invented.
- No claim of compromise, malicious attribution, financial loss, or business impact from suspicious URLs, shared IP values, or resolver failures.
- **No alert flood**: this version does not post unsolicited CISA comments or automatically send priority alerts.

## Build and qualification

Requires development headers/libraries for `libcurl`, `json-c`, and pthreads on the Digit host.

```sh
cd modules/s2
make clean && make && make test
```

Before installing, inspect test output and dependency availability. Module internal qualification requires 10/10 passing and an executed passing negative check.

Installation, module activation, and live end-to-end acceptance are **not** established by source commits or local build alone. The Director controls installation and subsequent steps.

## Current limits

This **first collection milestone** maintains case identities, changed-source signatures, and summary counts. It does NOT yet archive the full original source payload per event, generate case-level investigative narratives, correlate independent log sources, or call Ollama's `digit.llm.generate` service. These are additional requirements of the established S2 mission, not features claimed to be implemented. It also does not establish Sentinel API feed freshness or completeness: those must be verified against live runtime results.

The reporting command displays aggregate evidence only, not an incident verdict. Source timestamp timezone and top-level versus details IP roles remain unresolved until authoritative schema clarification. Avoid deploying for final incident adjudication on this version.
