# Digit Core Qualification — Current Evidence Boundary

**Date:** 2026-10-27  
**Status:** Core qualification NOT established for the current source revision.

## Mandatory rule

- Every revised Core requires full qualification: **minimum 20 mandatory Core behavioral tests; all applicable tests pass**.
- A successful build is not qualification.
- Module qualifications (IRC 20/20, Ollama 10/10) are independent of Core qualification.
- The historical October 19 Core 20/20 evidence applies only to the revision tested then.

## Restored source regression gate

Run from the Digit repository with the external STN-LABZ ABI sources available:

```sh
make test
```

This compiles the current Core and executes **20 independent source-contract regression checks**, plus **20 executable C unit-level Core behavior cases** and **20 shared-object hotload integration assertions** covering Root identity and fail-closed policy evaluation. The executable test uses isolated temporary logs and never touches the installed Digit service. The source checks in `tests/test_core_contracts.py`.  inspect lifecycle, routing, authorization, logging and hotload safety code contracts. The C unit tests do execute Root and policy implementation behavior, including negative conditions; the real shared-object integration harness injects qualification rejection and candidate start failure, verifies rollback restoration and successful replacement, and checks module logs. It does not establish uninterrupted service under concurrency or comprehensive Core qualification. A source-contract check passing is not an operational qualification receipt. This gate must never be reported as Core qualification.

## Required Core qualification work remaining

Additional integration qualification must cover actual Core scenarios not yet verified by the targeted rollback harness, including real success and negative paths for: Root acceptance/rejection, logging failures, discovery, registry state transitions, qualification threshold/negative validation, activation authority, module start and stop, rollback after failed replacement activation, safe unload under concurrent requests, route registration/withdrawal and errors, and integrity of installed module candidates. Additional tests remain required where engineering evidence warrants.

Record for each qualification run: exact Core commit and build hash, compiler output and warning count, external ABI revision, test suite revision, 20-or-more scenario results, negative cases, failure-injection evidence, and operator acceptance. Do not deploy the revised Core as qualified based only on `make test`.

## Runtime evidence separately observed

On 2026-10-27 operator logs confirmed IRC 1.1.0 module internal qualification 20/20 and Ollama 1.0.4 module internal qualification 10/10; both were activated. These are **module**, not Core, qualification results.

## New hotload integration gate (2026-10-27)

`tests/test_core_hotload.c` runs against compiled real test modules from `tests/fixtures/hotload_fixture.c` with disposable private test directories and audit logs. It verifies initial admission, rejection of 9/10 qualification, preservation of the previous ACTIVE module, start-failure rollback, successful new revision activation, loader ownership and record uniqueness, and shutdown. Source build uses an explicit **test-only** private staging override; deployed Digit does not read that override.

The 20 integration assertions are a supplemental gate, **not** proof of hotload thread-safety or Core production qualification. Actual results and warnings are pending operator execution of `make test`. Avoid implying that a synthetic start-failure fixture proves real-world rollback across all module implementations.
