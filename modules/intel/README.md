# Digit Intel 0.1.0 — independent intelligence collector

Mission: autonomously collect and correlate Sentinel observations and bounded Rictus logs; preserve high-relevance provisional investigations and state history. This module is **not** Slack, IRC, Router, or Core. It exposes **no** public collection command.

Reference: historical `stnlabz/digit/modules/stn2` investigation and correlation algorithms, adapted to the frozen current ABI 1.5. Prior work remains source material, not acceptance evidence.

## Private configuration and existing state

Uses the existing `/opt/digit/auth/intel/intel.conf` (mode 0600):

```ini
enabled=true
rictus_log=/var/log/rictus/rictus.log
api_base=https://api.stn-labz.com
advisory_list=/opt/digit/auth/intel/advisories.list
```

**Do not delete or reset** `/opt/digit/auth/intel/intel-state.json`, `investigations.json`, or `threat-ids.json`. The implementation continues the original JSON layouts. Existing state must be backed up before live activation; invalid/unsafe state must be treated as a collection failure and investigated, not silently replaced.

Create the private evidence/report directory before activation:

```sh
sudo install -d -o digit -g digit -m 0750 /opt/digit/intel
```

The module writes an append-only (normal filesystem append, not tamper-evident) `/opt/digit/intel/collection.log` and atomically replaces `/opt/digit/intel/latest-brief.md` after each collection. It collects on startup and approximately every five minutes while active. It tracks source retrieval gaps instead of pretending missing data succeeded. Source IP correlations are provisional, not proof of compromise.

The first revision **does not post to Slack or IRC**. Approved transport modules do not yet expose an intelligence outbound delivery service; adding such a capability requires its own authorized transport change. This revision does not change the accepted Slack or IRC modules. Do not treat this revision as complete Intel mission capability.

Build: `make -C modules/intel clean all test`. Install after tests: `sudo make -C modules/intel install`; restart only when authorized. Verify qualification, active runtime, and then collection log/report/state before operator acceptance. Network collection requires API reachability and read access to Rictus logs.

No case deletion or autonomous enforcement. Human operator retains command authority.

## Outbound delivery — revision 0.1.1 (awaiting acceptance)

Intel uses Core's existing ABI 1.5 service invocation to publish full reports through `slack.intel.publish` and concise summaries through `irc.intel.publish`. No inbound intelligence commands, Core changes, or Router changes. Slack targets only its configured `intel_channel_id`; IRC targets its existing configured channel. Delivery attempts are appended to `collection.log`. IRC queued means accepted into the transport's volatile one-slot queue, **not confirmed on-channel delivery**. Failed deliveries are retried; unchanged successes are suppressed for six hours while the process stays active. A restart resets this in-memory deduplication, and no durable delivery outbox exists yet.

The audit line delimiter and investigation section lookup were corrected. **Historical-state file readability/schema remains subject to live verification**. Valid JSON through `sudo` does not prove the `digit` account can read these `0600` files; verify access and ownership without replacing existing records. Absence of an investigation summary is now identified as unavailable in IRC, rather than claiming zero active cases.

Build all three changed modules with `make -C modules/{intel,slack,irc} clean all test` (in a shell loop); do not deploy unless all builds and tests succeed. Versions: Intel 0.1.1, Slack 0.1.2, IRC 0.1.2. Separate acceptance required for changed transports.

## Intel 0.1.2 — reporting control (pending acceptance)

Collection continues every ~300 seconds and refreshes `/opt/digit/intel/latest-brief.md`. Routine *outbound* full briefs to Slack and concise IRC summaries are time-gated to no more than once every 12 hours per transport; timing is persisted in `/opt/digit/intel/delivery-state.txt` (private 0600) so restarting Digit does not replay routine briefs. This new delivery file must not replace or reset existing investigation evidence JSON.

New investigation evidence/case changes may trigger a short Slack alert between briefs; the initial sample is treated as a baseline. The Core ABI service `intel.report.read` returns the latest stored bounded brief. Ask 0.1.8 exposes `report intel` only when the trusted Slack transport identifies the configured operator user. No collection command is exposed publicly. This is a read of the latest brief, not an unscheduled full collection.

## Architecture repair note (2026-10-14)

Intel remains an independent collection and reporting module. Its local `intel.report.read` service is a module-published read-only capability, not a Core-specific modification. The prior Ask-level on-demand report integration was withdrawn to restore the original shared Ask context ABI. Until a module-local route is separately qualified, `@Digit report intel` is **not** an accepted supported user command.
