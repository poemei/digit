# Digit ST-1 Research Registry — 0.1.1

**Engineering status:** Corrected source candidate. Operator end-to-end acceptance is OPEN. No live activation or production installation has been verified.

**Authoritative repository:** `poemei/digit`

## Purpose

Native, bounded evidence registry for external ST-1 research concerning Ezra, Sentinel-Daemon (SD), flight hardware, communications, and spacecraft operations. Research does not override the ST-1 planning hold or authorize engineering requirement changes, procurement, or spacecraft actions.

## Digit compatibility and boundary

The actual Core startup host in `poemei/digit/core/digit.c` exposes `register_service`, `unregister_service` and `invoke_service`. It does **not** expose module command-registration callbacks. This module registers `st1.query` and a manually invoked, source-attributed `st1.report` service. It invokes the existing Slack module's `digit.slack.st1.deliver` ABI service; no Core or Router modifications are required. No module-specific logic has been added to Router or Core.

The prior advertised `/st1` command was incompatible with the actual running Core contract and **is not a supported console route in this revision**. Free-form Digit conversation retrieval is also not connected to the service. A future integration requires inspection and authorized changes to the relevant module controller, not an unauthorized Core patch.

## Build and verify

From the repository root:

```sh
make -C modules/st1 test
```

This checks against `core/stnlabz_abi/module.h`, builds `st1.so` and a local research-management CLI, then executes C fixture checks and a shell smoke test. Building does **not** install or activate the module. Internal qualification contains 26 checks, with operator acceptance separate.

## Record schema

Fields: ID, record kind (PAPER / REPORT / STANDARD / MISSION), title, HTTPS source URL, publication date, summary, evidence (Verified / Reported / Inferred / Unknown), relevance (High / Medium / Low), affected systems, impact (Reinforces / Challenges / PotentialChange / None), mission milestone, monotonically increasing record revision, and UTC recording timestamp.

The append-only registry preserves earlier revisions. Changing evidence from Reported to Verified records a new version; it does not retroactively change old observations. A classification is an operator input, not independent source verification.

Default storage is `/opt/digit/research/st1/research.tsv`. The CLI refuses malformed fields and unsafe symbolic-link destinations. The file is private and requires appropriate local operator permissions.

## Local CLI operations

Only execute these after deciding where the research file should live, and arranging authorized filesystem access. For development use, override `ST1_PATH` at build time in an isolated directory rather than writing live data:

```sh
make -C modules/st1 ST1_PATH=/tmp/st1-review/research.tsv all
modules/st1/bin/st1_manage upsert ST1-1 MISSION "Relay demonstration" "https://example.org/report" "2026-10-25" "Reported milestone" Reported Medium communications None "Public demonstration"
modules/st1/bin/st1_manage list
modules/st1/bin/st1_manage watchlist
modules/st1/bin/st1_manage search relay
modules/st1/bin/st1_manage show ST1-1
modules/st1/bin/st1_manage history ST1-1
```

The example is **synthetic test data**, not an asserted real-world research finding. All writes occur through the local CLI, never via the read-only ABI service.

Each search returns at most eight records; pagination is not in this version. A missing or malformed registry is reported as a failure, not successful empty retrieval.

## Installed capability

If and only if the operator explicitly authorizes installation, use the module's `make -C modules/st1 install` target. That target installs the shared library, manifest, and local management utility; it does not itself attest to module discovery, qualification, activation, or console availability. The `st1.query` ABI request/result structures are defined in `includes/st1.h`. A qualified authorized consumer can invoke this service through the existing Core host service boundary.

## Acceptance gates

1. GitHub source revision confirmed.
2. Linux build and fixture tests run, exact results retained.
3. Core module discovery and qualification observed.
4. Runtime service registration and activation confirmed.
5. Operator-selected end-to-end retrieval tested, including unknown-ID and invalid-input cases.
6. Explicit human acceptance.

The operator designated Slack channel `C0CBYDEKK6J`. Slack's protected production configuration must contain `st1_channel_id=C0CBYDEKK6J`, its bot must have posting permission, and Slack module v0.2.1 must be active. `st1.report` takes zero request bytes and reports up to eight current, source-attributed registry records to that dedicated Slack ABI transport. Empty or unavailable research data, missing transport, and posting failures return errors: none is mislabeled as successful delivery. The report is **not** automatically scheduled, and no channel is directly accessed by ST-1 outside the ABI. Triggering the reporting service requires an independently authorized ABI consumer; no public `/st1` command is claimed. No claim of scheduled reports, automated ingestion, or free-form language integration is made.

**AWAITING OPERATOR END-TO-END TESTING AND ACCEPTANCE**
