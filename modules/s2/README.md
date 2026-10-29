# Digit S2 — Sentinel Intelligence and sai-dsa Publication v1.1.0

## Operating scope

Native C S2 module. Core and shared ABI are unchanged. S2 gathers Sentinel source observations every four hours and, when a valid public report is available, attempts to update the existing STN-LABZ `sai-dsa` filesystem-backed Markdown page on a six-hour cadence.

The public output is prepared by `src/s2_public.c` from validated Sentinel JSON. It contains the operator-requested headline and numbered investigative sections, source severity counts, available HTTP response evidence, explicit evidence gaps, an investigative assessment, and a **business-effects paragraph as the final section**. It does not export arbitrary source text, IP addresses, credentials, request details, domain identities or internal procedures.

**Evidence limitation:** The source currently owned by S2 is Sentinel plus a persisted case ID/hash register. Rictus event grammar, independent correlation, individual investigative narratives, complete durable case-event history and verified per-case chronology are not available in this module. Consequently the public report explicitly withholds unsubstantiated historical groupings and Rictus matches; the richer example report cannot yet be reproduced. Do not represent the generated brief as a completed cross-source investigation.

## Site login and authorization

The native publishing client `src/s2_publish.c` uses libcurl, HTTPS certificate validation, cookies in memory, the current ChAoS MVC login and CSRF forms, and the existing `/admin/page` update endpoint. It reads `/opt/digit/auth/sites/login.conf`, requiring a regular file accessible to the running Digit user and permissions not permitting group/other access. Never place those credentials in GitHub or logs.

Publication is restricted in the client to the existing page `sai-dsa`. Website access is still independently controlled by the server's Level 7 module permission and Mission Control gates. Digit must have explicit active assignment for Pages; login alone is not authorization. Neither the web Core nor the IRC module is modified.

S2 retains the previous published report if source retrieval, report validation, authentication or publishing fails. Publish success is tracked in `/opt/digit/state/s2/last-published`, and attempts are rate limited after failures. **Publication is enabled by this source when the module is installed/activated; inspect and qualify it before installation.** No claim is made that the live page was changed.

## Operator qualification

```sh
make -C modules/s2 clean all test
```

After passing module self-qualification, operator activation and authorization:
- Confirm the page is editable for Digit's assigned role and that the protected login file is readable by the service account.
- Review S2's prepared public brief against a known Sentinel fixture, including cases containing private IPs, URLs, and secret strings; ensure those fields never appear.
- Verify actual `sai-dsa` publication and page revision timestamp; verify no other page changes.
- Verify a 6h interval and retry behavior, plus invalid CSRF, unavailable Sentinel and denied Pages mission conditions.
- Confirm the last paragraph discusses **potential** business impact, not an invented loss or breach.

**Source committed; local build, module qualification, site authentication and live publication not verified.**

## Previously established operation

- Sentinel endpoint: `https://api.stn-labz.com/threats`
- Four-hour Sentinel poll, 8 MiB bounded ingest, source ID/hash register at `/opt/digit/state/s2/sentinel-register.txt`
- Retain absent records rather than silently closing cases; historical bulk ingestion is not a new attack.
- `/s2` provides status; quiet IRC synopsis for material changes remains.
