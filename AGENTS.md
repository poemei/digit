# Digit engineering instructions

## Scope and architecture

Read README.md and docs/ROADMAP.md before changes. Work on one operator-authorized step at a time. Preserve Core -> Dispatcher -> Modules, with Rendering as View. A roadmap is an ordered plan, not permission to implement future steps.

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

For foundation work before an APP/Slack path exists, report that limitation. The operator must explicitly select and accept a suitable current-stage demonstration (for example, a Core console run); the assistant cannot waive the gate or declare module tests an equivalent on its own.

Failures keep the current step open. Address only repairs within the authorized scope, then return the affected behavior for operator testing. Preserve failed evidence.

## Enforcement boundary

These are repository instructions for human and AI contributors, including ChatGPT and Codex. They do not claim that Digit's module manager or GitHub mechanically enforces human acceptance. Do not add such runtime enforcement, modify Core/ABI, or expand the architecture without a separate operator instruction.
