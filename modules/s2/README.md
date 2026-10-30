# Digit S2 — Sentinel Intelligence and sai-dsa Publication v1.2.0

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

## Public storytelling and source research — v1.2.0

- Sentinel's HTTPS JSON feed remains S2's factual basis. The public brief explains observations, the investigations S2 has actually performed, and what is not yet known in plain language suitable for consumers and businesses.
- S2 can read the operator's **existing** bare-text Project Honey Pot HTTP:BL key at `/opt/digit/auth/httpbl.key`. The secret is not copied into source, reports, logs, or GitHub. It requires regular-file ownership by the running service user (or root for a root-run service), plus private permissions and readable access. Earlier `httpbl.conf` / `module.conf` key-value locations remain fallback paths.
- HTTP:BL research is limited to three public IPv4 addresses per report preparation. It distinguishes search-engine classification from suspicious visitor, email harvester, and comment-spam indicators. Failure to resolve does not mean an address is safe; a classification does not establish an attack.
- An optional plain-English reader explanation is requested through the **existing** `digit.llm.generate` service. The request contains no raw source evidence or keys. Output is short, single-sentence, filtered for case-specific claims, and optional. S2's authoritative factual findings, investigative conclusions, and final business paragraph are untouched. Ollama outages do not block publication.
- No new module, independent scheduler, Core service, IRC change, or external Python process was introduced. Publication cadence remains six hours.
- **Unverified:** Local compile, qualifications, production key-file accessibility, HTTP:BL service responses, Ollama/editorial behavior, and live SAI-DSA publication. S2 does not yet implement historical HTTP:BL reputation caching, deep Rictus correlation, or the full investigative case history in the sample report.

Operator test: `make -C modules/s2 clean all test`. Activation and end-to-end publication remain under operator control.

## Operator-controlled live Pages integration check

Run from the Digit repository on the authorized host:

```sh
make -C modules/s2 test-live-page
```

**This command deliberately changes the live public page once.** The separate native C test (`tests/test_sai_dsa_live.c`) reads existing OS-owned website credentials without printing them; checks the login form and CSRF; authenticates; loads the `sai-dsa` editing page; preserves its existing title, description and full Markdown content; appends one visible UTC-dated publishing-connectivity test marker; submits the edit through the existing protected Pages endpoint; then reopens the edit form and compares the persisted text with the requested text. No test is run by `make test`, `make all`, hotload, or the automatic S2 worker. An actual run is required for proof; a successful redirect alone is not proof of saved content.

The test reports the first failing stage without revealing credentials. It does not modify other pages, erase existing content, or alter automatic S2 cadence. Run only when the operator intends to make the public change. Authorization is enforced by the existing website permissions; successful login is not assumed to imply Pages access.
