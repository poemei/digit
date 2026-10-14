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
