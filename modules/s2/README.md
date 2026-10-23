# Digit S2 v0.3.1 — Consistent Intelligence Report Dividers

S2 uses the existing ABI 1.5 services and is hot-loadable without restarting Digit. This revision routes evidence-bounded, six-section assessment briefs and Rictus-linked observation updates through the **existing** `digit.slack.intel.deliver` service. Slack itself owns the configured private `intel_channel_id`. Neither Core nor Slack needs modification.

The six report sections are Executive Summary, Active Investigations, Threat Intelligence Overview, Cross-Source Intelligence, Intelligence Gaps, and Final Assessment. S2 preserves uncertainty: a Rictus/Sentinel indicator overlap is not a proven compromise. Unknown case counts and absent investigative history are explicitly marked **not established** rather than fabricated.

Rictus log `/var/log/rictus/rictus.log`, Sentinel feed `https://api.stn-labz.com/threats`, and the existing S2 observation service remain the evidence inputs. Source-status changes and relevant indicator changes generate structured briefs; the existing short IRC status pathway remains separate.

**Scope limitation:** This revision implements private distribution and six-section formatting, not a persistent case engine, historical correlation, source timestamp normalization, full 24-hour metric aggregation or evidence-backed investigation IDs. Those fields cannot be honestly populated until source grammar and case persistence are implemented. S2 does not publish private findings to websites or bridge channels.

On observation updates, private Slack delivery is attempted before advancing the observation cursor. On a failed private delivery, the record is not acknowledged. For general briefs, the module maintains an in-memory pending brief for retry; it is not crash-durable. The existing IRC status messages remain short and do not include raw logs.

Use Digit's **existing hot-load lifecycle** after building/installing this module. No Digit restart, no Core change, no changes to the held public Intelligence/SRT modules. Offline qualification checks format and negative cases; it does not prove live private Slack delivery.


## MEDIUM investigation trigger (v0.1.7)

MEDIUM/MEDIUM or higher severity **and** relevance starts an investigation immediately, without waiting for HIGH. Every qualifying Sentinel source ID is stored in SQLite at `/opt/digit/state/s2_cases.sqlite3`, preserving its original ID, original timestamp (timezone not inferred), primary and details IP fields separately, domain, request URL, severity/relevance and last observation. Subsequent changed evidence updates the **same** case. Escalation is only labeled when recorded severity increases; merely expecting a future HIGH event does not promote it.

The active case count and up to five case changes per polling interval are included in S2's private Slack six-section report. This is a bounded, persistent **source-record case register**, not yet a full analytical investigation engine or independent proof of malicious activity. Cases are keyed by source ID and are not automatically merged on shared IP. Repeated unchanged API snapshots do not increment case revisions. SQLite must be available, and the existing Digit process must be permitted to create the case database under `/opt/digit/state`. Failure is reported as a persistence warning rather than silently claiming success.

Operational limitation: the existing brief retry buffer is in-memory only. A change committed to SQLite could lose its unsent Slack notification across a process exit; there is no durable Slack outbox yet. This patch does not change Core, Slack, IRC, the SRT module, or website publishing. Build/install using Digit's established hot-load lifecycle; no service restart is required.

## v0.1.9: Slack presentation and case-report corrections

The title is now separated from its organizational masthead with a blank line and is rendered with Slack's single-asterisk bold syntax. The six section headings have explicit spacing. Sections 1 and 2 report the real persistent SQLite source-record count and current ingestion changes, rather than claiming that no case register exists. Section 3 consistently states the MEDIUM/MEDIUM investigation threshold. Initial bulk imports are identified as historical ingestion, not new attacks. Unknown case metrics are marked unavailable rather than invented.

The source is committed but neither compilation nor live Slack formatting is independently verified by the repository update.

## v0.2.0: S2-to-IRC intelligence snapshot

For each successful private Slack S2 report delivery, S2 sends a bounded one-line executive snapshot via the already existing `digit.irc.deliver` service. The message ends with `Full intelligence brief: Slack -> #stn-labz`. The full six-section evidence report remains private in Slack. A failed Slack post is not advertised as successful. Failed IRC notice delivery is retried in process; no durable queue is claimed. Observation intelligence updates also advertise their completed private Slack report.

This is **S2 report distribution**, not a restored IRC↔Slack general-chat bridge: no Slack chat is forwarded to IRC and no IRC chat is forwarded to Slack. Neither IRC nor Slack modules, Core, nor the Intelligence/SRT web modules are changed. Existing ABI 1.5 service names and hot-loading remain unchanged.

## v0.2.2 — Evidence-backed case changes

The existing per-source `s2_cases` snapshot now has an append-only `s2_case_events` history recording `OPENED`, `ESCALATED`, `DECREASED`, and `EVIDENCE_UPDATED` transitions. Every transition is tied to the source ID, source-supplied timestamp, both IP fields, domain, request URL and the observed severity/relevance. The snapshot and history insert use one SQLite savepoint, so a partial history write does not silently update the case. Identical repeated feed snapshots do not create events. Changes to fields other than severity/relevance are now detected, including revised IPs, domain and request URL.

The previous MEDIUM/MEDIUM combined investigation threshold applied through v0.2.2. A decrease in source-provided scores is recorded only while both remain at least MEDIUM; falling below the threshold, case closure, confirmed incident status, temporal Rictus correlation, and independent attribution are **not yet implemented**. The initial import reflects historic source records, not newly occurring attacks. No automatic public publishing or new Core/Slack/IRC dependencies are introduced. Run `make -C modules/s2 clean all test` before operator-controlled installation/hotload. This source change has not been live-qualified.

## v0.2.3 — Source severity controls investigation

The STN-LABZ `/threats` API `severity` field opens or maintains investigation for **MEDIUM, HIGH, or CRITICAL** without requiring the separately supplied `relevance` field to be MEDIUM. Relevance is preserved as evidence/context and never vetoes those severities. Missing/LOW severity does not qualify merely because relevance is high. S2 qualification tests each boundary; unchanged snapshots remain deduplicated, and correlation is never reported as proof of compromise. This revised threshold may admit previously excluded source records as historical imports, not newly occurring attacks.

## v0.2.4 — Investigation case reviews in private Slack

S2 now appends a bounded review of up to five highest-severity stored investigations to generated private Slack intelligence briefs. For each review, the source record ID, API severity, relevance, source timestamp, primary/detail IPs, domain, requested URL, revisions and last recorded transition are read from the SQLite case register and its evidence history. Assessment and next-action language is explicitly qualified; this is a prioritized evidence review, **not** independent incident confirmation or proof the listed historical event just occurred. Line spacing in the core brief was improved. No automatic website publication, Core change, or IRC detail dump is introduced. Cases are displayed when S2 has generated a brief; this is not a new periodic reminder schedule. Live qualification and delivery remain to be verified.

## v0.2.5 — Bounded correlation leads and chronology

The private Slack review now selects up to three highest-severity source records in each of two sections: source timestamps comparable to the last 24 UTC hours and older source records. The date comparison is **provisional**: the API's source timestamp timezone is unverified; a date in the recent section is not confirmation of a recent attack. Reviews display all available case fields, the case change count and last event, and counts of other stored records sharing a primary IP or domain. Shared indicators are investigative leads only and do not establish the same actor, incident, or compromise. Missing evidence is labeled `not supplied`; specific next actions depend on whether an IP, domain, or neither can be compared. Source records are not automatically publicly published. The generic IRC summary and Core are unchanged.

## v0.2.6 — Indicator roles remain unknown until verified

The case review distinguishes the API top-level `ip` from `details.ip_address`, explicitly preserving their source-field identity. It calculates separate case-register overlap counts for each field and for domains; none of those counts verifies what an address represents, a common actor, or compromise. When repeated details IPs exist, it calls for establishing that field's role; when the same top-level IP appears broadly, it asks for source semantics (server, proxy or client) before attribution. A shared domain similarly remains an investigative lead, not a confirmed incident. Slack formatting separates individual case evidence and assessments into legible paragraphs. No Core or IRC changes, no public publication. Live compilation and investigative validation are still required.

## v0.2.7 — Consolidated investigative leads

The private Slack case review no longer expands six nearly identical full case cards. It groups up to three leads in each provisional recent/historical source-time window, using `details.ip_address` if supplied, otherwise `domain`, otherwise the API's top-level `ip` (role unknown). Each lead lists the number of matching records, highest reported severity, number of revised case records, first/last source timestamps, example source IDs, and a field-specific next investigative action. Shared field values are not proof of a shared attacker, incident or compromise. Source timezone remains unverified; historical case baseline means no subsequently recorded transition, not necessarily an old attack. This change does not create automatic website publication, new log ingest, or independent verification of source field semantics. Core, IRC and Slack modules remain unchanged. Qualification and production behavior must be verified on Digit's host.

## v0.2.8 — Distinguish findings from indicator groups

Private Slack reports now explicitly mark a single source record as **INDIVIDUAL FINDING** and two or more records sharing the chosen source-field value as **SHARED-VALUE GROUP (unverified)**. Grouping is descriptive, never proof of a verified indicator, shared actor or incident. Each bounded result includes a source domain and request-path example when available, severity, source time span, revisions, representative source IDs, and a field-specific investigative next step. Blank fields are marked 'not supplied'. Reports use stronger paragraph spacing and limit display to four findings per provisional source-time window. The source timezone remains unverified; the case database, private Slack transport, IRC behavior and Core remain unchanged. Live qualification is still required.

## v0.2.9 — Time-bounded shared-value groups

S2 now groups records by **source field, identical indicator value, and the same source calendar date** (first 10 characters of the source timestamp) instead of grouping all historical records across months. This conservatively prevents distant dates from being presented as one shared-value group. Cases on opposite sides of midnight remain separate even when close in time; this is a date-window safeguard rather than complete event-time correlation. The API timezone remains unverified, so do not infer UTC incident windows, confirmed relationships, or independent attribution. Individual MEDIUM/HIGH/CRITICAL source findings remain eligible for reporting. No case data is deleted or merged; SQLite history and private Slack/IRC transport remain unchanged. Compilation and live qualification remain to be verified.

## v0.2.10 — Change-driven reporting

S2 continues polling and keeping its SQLite source/case history, but suppresses repetitive full Slack briefs when case state, source accessibility, exact Rictus correlation status, and persistence health remain unchanged. A full brief is still generated on first activation, new/changed qualifying source cases, API or Rictus availability transitions, correlation state changes, and case persistence failures or recovery. Changes to the raw log-record count and repetition of an unchanged latest source signature no longer trigger a full report. Observation reporting uses its existing independent pipeline. This is an in-memory delivery gate: a hotload/restart will produce a new initial baseline, and failed private Slack delivery still follows existing pending-message behavior; the change does not implement a durable outbound notification queue or a scheduled status digest. No Core, IRC transport, or public publishing changes. Production qualification is still required.

## v0.3.0 — Protected http:BL lookup (IPv4 only)

S2 reads the 12-letter lowercase Project Honey Pot access key from `/opt/digit/auth/httpbl.key` as the Digit runtime account (expected `digit:digit`, file `0600`, directory `0700`). The key is not committed, printed, or included in reports. On a meaningful S2 private-intelligence update, the module performs at most three DNS-based reputation lookups for distinct qualifying `details.ip_address` IPv4 values; no IPv6 lookups are attempted. The result is appended privately to the Slack brief as `HTTP:BL ENRICHMENT`, reporting listed, not listed, key unavailable, or lookup incomplete. A listed response reports age in days, threat score, and type bitmask. A non-listing is **not an all-clear**, and every lookup is external reputation evidence, not proof that the field is the client, source, attacker or destination IP or that a compromise occurred. `getaddrinfo` uses the host's normal DNS resolution; no extra HTTP authentication header or public release is involved. Live DNS, credentials, build, hotload and report delivery are **not yet verified**. No Core, Publisher, website or IRC modifications were made. Version follows STN-LABZ rollover 0.2.10 → 0.3.0.

## v0.3.1 — Slack report horizontal separators

Every finding in the recent and historical case sections now begins after a full-width divider (`━━━━━━━━━━━━━━━━━━━━━━━━━━━━`). Matching dividers separate the six established intelligence sections, the investigation findings section, http:BL enrichment, and optional case-activity/persistence warnings. The http:BL heading and reputation results have explicit paragraph breaks. The threat-intelligence threshold statement no longer runs into the next field. This release changes private Slack presentation only; source investigation, credential lookup, persistence, IRC, publishing, and Core remain unchanged. Source code has been committed, but build, qualification, and visual verification in Slack remain to be performed.
