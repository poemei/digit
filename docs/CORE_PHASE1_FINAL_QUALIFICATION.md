# Digit Phase 1 — Final Core Qualification Review

**Repository:** poemei/digit  
**Review basis:** accepted Core consolidation revision acc346aa073413efa0b0b1d5e4cf58ad562b36c3; source observed on GitHub during this review  
**Disposition:** PHASE 1 EXIT NOT YET QUALIFIED. No Phase 2 authorization implied.

## Evidence categories

Previously operator-confirmed: `make clean && make && make test` completed with **14/14 suites** and **1327/1327 assertions**, with successful zero-module console startup and controlled shutdown. This is evidence for the accepted consolidation revision, not a newly executed test run for this review.

Source-inspection findings in this review do not constitute a compiler run or runtime demonstration.

## Phase 1 requirements crosswalk

| Roadmap Phase 1 requirement | Evidence and qualification |
| --- | --- |
| Permanent identity and foundational function | Digit's compiled Agent Root and three exact General Orders are validated in `src/agent_root.c` and `src/identity_digit.c`. **Supported for tested foundation**. |
| General Orders at Core level | `digit_core_policy_evaluate` maps mission, policy/instruction, and delegated authority decisions to GO1/GO2/GO3. Missing context denies. **Supported as internal policy evaluation**, not complete authority provenance. |
| Policy and authority enforcement | `digit_core_authorize` checks the policy evaluator. However the public shared ABI function `stnlabz_module_abi_authorize_and_activate` calls `stnlabz_module_registry_authorize_activation` itself, without invoking Core's policy evaluator. The registry authorization function is also public. **NOT PROVEN across all reachable execution paths**. No external attack surface is established by this observation. |
| Denial and escalation | Denial and `ESCALATE` classification are tested in `tests/test_policy.c`. Escalation writes a Core log event; it does not transmit a report to a configured next authority. **Partial**, with delivery unresolved. |
| Independent startup, shutdown and operator status | Previous operator console evidence shows normal zero-module startup and graceful stop. **Supported for tested scenario**. Failure conditions are partly covered by existing tests. |
| Module ABI, service contract and lifecycle | Shared ABI 1.5 descriptors, registry and lifecycle interfaces exist, with compatibility tests. **Supported for tested contracts**. Do not change shared ABI without separate authorization. |
| Module registration, discovery, isolation, failure outcomes | Discovery, rejection, registry growth and failure/stop tests exist. No production external module was activated in the operator run. **Supported in mock/fixture coverage; live module evidence absent**. |
| Qualification and admission | Qualification, negative evidence and activation refusal are exercised in existing suites. **Supported in available tests**. |
| Trace metadata contract | Operator expressly assigned detailed diagnostic tracing to the future DSD module and directed removal of Core trace subsystem. Core keeps operational logs; DSD metadata/correlation implementation is **deferred by operator architecture decision**, not silently counted as complete. |
| Unit/negative tests | **1327/1327** accepted baseline; source inspection alone is not fresh execution. |

## Concrete findings and open qualification blockers

1. **Authority-path gap — highest priority.** Shared ABI entrypoint `stnlabz_module_abi_authorize_and_activate` independently grants registry activation authorization. That function does not call Digit Core's policy evaluator. Digit's intended callers currently use `digit_core_activate` and `digit_core_authorize`, but the public ABI path precludes claiming the Core policy boundary is universally enforced. Qualify ownership and trusted call reachability without changing shared ABI 1.5 by assumption. A future verified caller boundary must not accept untrusted authority booleans as proof.
2. **General Order 3 delivery.** The current Core evaluator categorizes out-of-authority operations and records an escalation target in the mandatory log. It does not establish delivery or acknowledgment by the appropriate next authority. Do not mark reporting fully verified.
3. **Legacy duplicate module-management implementation.** `src/module_manager.c` maintains a separate, fixed-capacity manager, while production `src/digit.c` uses the dynamic shared ABI registry. It is exercised by a dedicated suite but is not used on the production Core path. Establish whether it is retained compatibility/test infrastructure or requires removal under a separately scoped change; do not treat it as proof of the production manager.
4. **Failure-path nuance.** `src/digit.c` invokes some cleanup and logging operations without checking every return; existing tests support observed clean zero-module shutdown, not every possible I/O-failure outcome. Do not claim full fault-tolerance validation without targeted evidence.
5. **DSD scope boundary.** Core operational logging is preserved. Detailed diagnostics, trace correlation, source-line investigation and diagnostic reports belong to DSD. Do not reintroduce `core_trace.c` as a Phase 1 workaround.

## Recommendation at phase gate

**HOLD Phase 1 exit.** Earlier milestones remain individually accepted. Resolve or expressly disposition the authority-path and escalation/reporting gaps, determine the production relevance of the legacy manager, then perform targeted regression and operator-selected `make clean && make && make test`, `./bin/digit`, Ctrl+C, and `logs/digit.log` review.

No Phase 2 Bootstrap, Router, DSD module, transport or other capability implementation was authorized by this review. **Operator approval is required for Phase 1 closure and separate authorization is required for the next named step.**
