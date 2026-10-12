# Digit engineering instructions

## Scope and architecture

Read README.md and docs/ROADMAP.md before changes. Work on one operator-authorized step at a time. Preserve **Core -> Bootstrap -> Router -> relevant Module (Model) -> Rendering (View)**, following ChAoS MVC. Core contains Digit's identity, General Orders, foundational function, lifecycle, mandatory logging, and module management. Bootstrap initializes and hands off; Router only routes (e.g. `/ask` -> Ask module). Modules work independently and return results for rendering: never require module-to-module-to-module response chains. SA-specific authorization is a future module, not part of Core identity or a mandatory Core dependency. A roadmap is an ordered plan, not permission to implement future steps.

## Mandatory operator acceptance gate

**Build success, passing tests, qualification GREEN, module registration, activation, and HOTLOAD_ACTIVE are NOT operator acceptance and NEVER authorize the next development step.**

Keep these evidence gates separate:
1. Source change and exact revision/module version.
2. Compilation and relevant automated tests.
3. Module qualification/admission.
4. Runtime activation of the intended version, if authorized.
5. End-to-end operator testing through the operator-selected APP, Slack, console, or other actual client.
6. Explicit operator acceptance of that tested behavior.
7. Explicit operator authorization for the next named step.

An earlier gate never proves a later one. Never label the step complete or accepted solely because module management reports GREEN or ACTIVE.

## Required stop and handoff

After the authorized implementation and available engineering checks, report:
- Changed files, revision and module versions.
- Actual build/test results and failures; activation status separately.
- A short client test procedure: exact inputs, expected behavior, and relevant negative cases.
- Which checks remain unperformed, and any known limitations.

If operator testing or acceptance is outstanding, finish with **AWAITING OPERATOR END-TO-END TESTING AND ACCEPTANCE** and stop implementation. Do not start the next module, feature, migration, phase, speculative refactor, or background development while waiting. Do not repeatedly poll for approval.

The operator's test result belongs to the tested revision, version, client and scenario. Record expected versus observed behavior and the operator's explicit acceptance when supplied. Never invent this evidence. A code change affecting accepted behavior reopens its relevant acceptance checks.

A request to install, activate, hotload, test, diagnose, or fix the current step is permission for that requested action only. Silence, successful activation, a passing screenshot, or an automated test result is not permission to advance. Acceptance alone is not authorization for another step: wait for the operator to identify or explicitly approve that next step. One explicit operator message may both accept the current step and authorize the named next step.

**Current Core-stage selection:** Digit has no Windows APP or Slack client connection. The operator has explicitly selected Core compilation, the aggregate `make test` pass/fail report, Core console startup/shutdown, and `logs/digit.log` evidence as the applicable foundation-stage evaluation procedure. Present actual results from those checks and await the operator's explicit acceptance and next-step authorization. Do not require a nonexistent APP/Slack interface during Core development, and do not assume automated success alone is acceptance. Once a client exists, use the operator-selected end-to-end client checks.

Failures keep the current step open. Address only repairs within the authorized scope, then return the affected behavior for operator testing. Preserve failed evidence.

## Core logging requirement

Core logging is mandatory. In the current development build, write to `./logs/digit.log` relative to Digit's working directory (e.g. `~/digit/logs/digit.log` when launched there). Production installation is intended under `/opt/digit`; the working directory determines the local log path. Core events and future module diagnostics must use the common logging facility; do not introduce `audit.log`.

## Enforcement boundary

These are repository instructions for human and AI contributors, including ChatGPT and Codex. They do not claim that Digit's module manager or GitHub mechanically enforces human acceptance. Do not add such runtime enforcement, modify Core/ABI, or expand the architecture without a separate operator instruction.
