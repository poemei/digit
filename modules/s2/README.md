# Digit S2 v0.1.6 — Private Intelligence Reporting

S2 uses the existing ABI 1.5 services and is hot-loadable without restarting Digit. This revision routes evidence-bounded, six-section assessment briefs and Rictus-linked observation updates through the **existing** `digit.slack.intel.deliver` service. Slack itself owns the configured private `intel_channel_id`. Neither Core nor Slack needs modification.

The six report sections are Executive Summary, Active Investigations, Threat Intelligence Overview, Cross-Source Intelligence, Intelligence Gaps, and Final Assessment. S2 preserves uncertainty: a Rictus/Sentinel indicator overlap is not a proven compromise. Unknown case counts and absent investigative history are explicitly marked **not established** rather than fabricated.

Rictus log `/var/log/rictus/rictus.log`, Sentinel feed `https://api.stn-labz.com/threats`, and the existing S2 observation service remain the evidence inputs. Source-status changes and relevant indicator changes generate structured briefs; the existing short IRC status pathway remains separate.

**Scope limitation:** This revision implements private distribution and six-section formatting, not a persistent case engine, historical correlation, source timestamp normalization, full 24-hour metric aggregation or evidence-backed investigation IDs. Those fields cannot be honestly populated until source grammar and case persistence are implemented. S2 does not publish private findings to websites or bridge channels.

On observation updates, private Slack delivery is attempted before advancing the observation cursor. On a failed private delivery, the record is not acknowledged. For general briefs, the module maintains an in-memory pending brief for retry; it is not crash-durable. The existing IRC status messages remain short and do not include raw logs.

Use Digit's **existing hot-load lifecycle** after building/installing this module. No Digit restart, no Core change, no changes to the held public Intelligence/SRT modules. Offline qualification checks format and negative cases; it does not prove live private Slack delivery.

