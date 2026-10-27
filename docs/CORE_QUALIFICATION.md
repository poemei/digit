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

This compiles the current Core and executes **20 independent source-contract regression checks** in `tests/test_core_contracts.py`. The checks inspect concrete lifecycle, routing, authorization, logging and hotload safety code contracts. They do **not** exercise those runtime behaviors, inject failures, or establish Core 20/20 qualification. A source-contract check passing is not an operational qualification receipt. This gate must never be reported as Core qualification.

## Required Core qualification work remaining

A separate executable behavioral suite must test at least 20 Core scenarios, including real success and negative paths for: Root acceptance/rejection, logging failures, discovery, registry state transitions, qualification threshold/negative validation, activation authority, module start and stop, rollback after failed replacement activation, safe unload under concurrent requests, route registration/withdrawal and errors, and integrity of installed module candidates. Additional tests remain required where engineering evidence warrants.

Record for each qualification run: exact Core commit and build hash, compiler output and warning count, external ABI revision, test suite revision, 20-or-more scenario results, negative cases, failure-injection evidence, and operator acceptance. Do not deploy the revised Core as qualified based only on `make test`.

## Runtime evidence separately observed

On 2026-10-27 operator logs confirmed IRC 1.1.0 module internal qualification 20/20 and Ollama 1.0.4 module internal qualification 10/10; both were activated. These are **module**, not Core, qualification results.
