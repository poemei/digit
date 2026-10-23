# Digit S2 v0.2.3 — API Severity Investigation Gate

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
