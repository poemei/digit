# Digit Core Qualification — Evidence Record

**Recorded:** 2026-10-19  
**Scope:** Digit Core verification, not module qualification  
**Evidence:** Operator-supplied `make test` console output from the deployed development environment  
**Status:** Current Core test suites PASS; 20/20 tracked coverage milestones reached

## Result

The operator ran `make test` and supplied the final output:

```text
Completed checks: 20 / 20 required (100%; 0 remaining)
Agent qualification: CURRENT SUITES PASS (20 coverage milestones in 8 suites; see individual checks)
OVERALL TEST COMPLETED: PASS (current suites only)
```

This confirms the **current test suite milestone**, not an exhaustive proof that Core cannot fail. The 20/20 number is reported by the existing test runner as tracked coverage milestones; it should not be confused with the number of individual assertions or independent runtime failure scenarios.

## Core checks and receipts

| Suite / area | Verified evidence |
| --- | --- |
| Core reporting / identity | Existing Core reporting suite completed |
| Transport ABI boundary | 5 checks passed |
| Dual-stream logging | 1 check passed |
| Authorization integrity | 8 positive, 3 negative, 0 failures |
| Lifecycle integrity | 14 positive, 9 negative, 0 failures |
| Service registry protection | 7 positive, 7 negative, 0 failures |
| Hotload version guards | 7 positive, 6 negative, 0 failures |
| Operator command security | 5 positive, 6 negative, 0 failures |

**Additional Core qualification suites (authorization through operator commands):**
- Positive assertions: **41/41**
- Negative assertions: **31/31**
- Total assertions: **72/72**
- Failures: **0**

The positive/negative totals above apply to the **five newly introduced suites**, not the original reporting, transport, or logging tests.

## Negative verification and integrity

The isolated tests confirmed these scoped behaviors:

1. Unauthorized activation is refused and leaves the qualified registry state intact; stopping revokes activation authority, and valid subsequent reauthorization succeeds.
2. Invalid lifecycle transitions, duplicate qualification and verification, active unregistration, and quarantined reactivation are rejected without the targeted unauthorized state changes.
3. Duplicate or missing services are rejected, ownership is checked on unregistration, and the registry stays usable following rejection and service withdrawal.
4. Hotload version guards reject equal or regressed versions, preserve incumbent *tracking metadata* during rejected candidate checks, and compare major/minor/patch precedence correctly.
5. Core's management-message handler refuses unauthorized accounts and public-channel management requests, queues authenticated operator DM requests, rejects malformed or competing commands, and remains callable afterward.

**Important test boundary:** The Core authorization and lifecycle tests exercise the actual registry functions using isolated fixtures. The service-registry and command tests include the actual Core implementation and pass synthetic requests. The hotload guard test includes the real hotload implementation but exercises its version/tracking functions without triggering a complete replacement operation.

## Not yet demonstrated by these tests

- Full live hotload rollback after a candidate fails activation, including proof that the incumbent remains functioning.
- Verification that upstream IRC account authentication cannot be spoofed; the Core command suite assumes the supplied `account` field is trusted.
- End-to-end execution of a queued operator activation/deactivation command (the command suite verifies queueing).
- Every concurrency, service-withdrawal race, fault-injection, recovery, and long-duration operational scenario.
- Independent auditing of the test runner's 20-milestone classification. Some isolated fixtures manufacture qualification records for controlled conditions; those values are fixture inputs, **not** additional proof of the agent's own qualification.

These remain separate targets. Do not represent them as completed based solely on the 20/20 result.

## How to reproduce the current suite

From the repository root:

```sh
git pull
make test
```

Inspect the individual PASS/FAIL lines and the summary. The runner only emits the current-suites PASS summary when every configured suite exits successfully. This test command must not be confused with deploying modified Core binaries.

## Authority and change control

Core remains under explicit human project-director control. This documentation update records results supplied by the operator; it does **not** authorize altering, deploying, or independently restarting the Core.

**Engineering principle:** Show me das evidences. Report what passed, distinguish what remains untested, and do not substitute qualification counters for operational proof.
