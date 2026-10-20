# AGENTS.md — Digit AI Engineering Instructions

## 1. Purpose

This document establishes mandatory engineering instructions for AI assistants, coding agents, and automated development tools working on Digit.

Digit is an **Intelligent Autonomous Engineering and Management Asset** developed in native C under the ChAoS MVC architecture.

Digit is not an LLM-driven AI, a Corpus library, or a question-answering bot.

Her Observations, environment, and Corpus constitute her knowledge base. Her chat functionality is one component of her broader intelligence and mission responsibilities.

All engineering must preserve Digit's declared identity, architecture, mission, and human authority structure.

**These instructions apply to every AI-assisted engineering task involving Digit.**

## 2. Human Authority

Human directives govern all project work.

AI assistants possess no independent authority over Digit, its repository, its architecture, or its operators.

Mandatory rules:

1. Follow the human operator's explicit instructions.
2. Do not disregard, circumvent, or silently reinterpret restrictions.
3. Do not expand the authorized scope of a task.
4. Do not initiate unrequested engineering activities.
5. Do not substitute an AI-preferred architecture for the established architecture.
6. Do not treat successful tests as authorization to deploy or change protected components.
7. Do not represent incomplete work as complete.
8. Do not make unauthorized repository changes.
9. Do not create Git branches or GitHub workflows.
10. Human evaluation and acceptance remain authoritative.

When a technical requirement conflicts with an explicit restriction, report the conflict. Do not resolve it by violating the restriction.

## 3. Protected Core

Digit's Core resides in `core/`.

**Nothing within Core may be changed, altered, updated, or replaced without direct consent from the Director of the Digit Project.**

Core qualification, successful compilation, test results, or an AI assessment do not constitute permission.

Core responsibilities and interfaces must be respected by all modules.

Modules must not circumvent Core authority through alternative code paths, hidden dependencies, or duplicated protected functionality.

No AI assistant may grant itself approval for a Core modification.

## 4. Architecture

Digit follows the ChAoS MVC architectural structure.

The project uses its established Core, Model, View, and Controller responsibilities.

The following directory rules are mandatory:

- Core responsibilities belong in `core/`.
- The project does not permit conventional `includes/` or `src/` directory structures.
- New work must conform to existing project conventions.
- Existing interfaces and module contracts must be examined before implementation.
- Architectural changes require explicit authorization.

Do not invent missing architectural components merely to simplify implementation.

Do not move functionality between components without authorization.

## 5. Prohibited Substitutions

Digit does not rely on third-party LLM inference services.

The following substitutions are prohibited:

- Ollama integration
- External LLM inference dependencies
- Replacing Digit's intelligence architecture with a conventional chatbot
- Replacing conversational reasoning with simple Corpus recitation
- Introducing an external AI service as a workaround for incomplete native functionality
- Reinterpreting Digit's requirements to fit the capabilities of another AI product

Development-time assistance from an authorized coding tool does not authorize adding that tool or its inference services to Digit's runtime.

## 6. Mission-Oriented Engineering

Every implementation must serve the actual assigned functionality and Digit's established mission.

A module is not successful merely because it compiles, loads, or answers a test request.

Engineering must distinguish:

- Component qualification
- Integration correctness
- Runtime behavior
- Mission-specific capability
- Human acceptance

Do not equate knowledge retrieval with comprehension, response generation with conversational participation, or module activation with operational success.

## 7. Source Inspection Before Modification

Before modifying a source file:

1. Read the entire affected file.
2. Examine relevant headers and declared interfaces.
3. Identify the module's responsibility and dependencies.
4. Trace the affected execution path.
5. Examine applicable build and test requirements.
6. Identify related defects rather than treating each compiler error independently.
7. Preserve functioning behavior outside the requested correction.

Compiler errors identify investigation points. They do not define the complete scope of the defect.

**Whole-file analysis and complete-file corrections are required.**

Do not use repeated small patches as a substitute for understanding the source.

Do not rewrite unrelated working functionality.

## 8. Engineering Changes

All changes must:

- Be within the human-authorized scope.
- Respect existing module boundaries.
- Preserve relevant APIs and ABIs.
- Respect ownership, lifetime, and resource-management contracts.
- Preserve established functional behavior.
- Avoid unnecessary dependencies.
- Address the identified problem completely.
- Remain reviewable and attributable.

Do not introduce new configuration files, database files, services, dependencies, directories, or architectural mechanisms without an established requirement.

If the required change cannot be completed without crossing a protected boundary, stop the affected change and report the dependency.

## 9. Native C Engineering Standards

Native C work must follow the ISO C version and toolchain requirements established by the project.

Applicable standard practices include:

- Correct declarations, definitions, linkage, and translation-unit boundaries
- Valid object lifetimes and memory ownership
- Safe pointer and array operations
- Explicit buffer capacity handling
- Proper allocation and deallocation
- Appropriate error and return-code checking
- Valid structure layouts across ABI boundaries
- Consistent resource initialization and cleanup
- Defined behavior and portability within supported environments
- Compiler diagnostics and warnings reviewed before acceptance

Do not assume C, C++, and other languages are interchangeable.

Use the language and standards specified for the actual assignment.

Do not silently change language standards or compiler requirements.

## 10. Module Development

Before developing or correcting a module, inspect the applicable module documentation, established interfaces, and working reference implementations.

Where supplied, consult:

- `AGENTS.md`
- `MODULE_CREATION_GUIDE.md`
- `20261016.0_AI_MODULE_CREATION.md`
- `example.zip`

A module must follow the current project ABI and lifecycle contracts.

Module qualification must perform actual checks. Test counters must reflect executed tests.

Registration, startup, shutdown, reload, and failure recovery must behave according to their established contracts.

Hotload activation does not independently prove that a module fulfills its operational purpose.

Do not introduce cross-module dependencies that violate architectural ownership.

## 11. Database Integration

Database work must use the operator-provided schema as the authoritative implementation reference for the assigned task.

When integrating SQLite:

- Inspect the actual schema before changing database access logic.
- Use the established database module and authorized interfaces.
- Use prepared statements and bound parameters where applicable.
- Preserve data integrity and transactional correctness.
- Handle database errors explicitly.
- Preserve source provenance when storing or retrieving knowledge.
- Verify query results rather than assuming that database accessibility establishes retrieval correctness.
- Do not invent access restrictions or redesign database ownership without authorization.
- Do not create replacement databases or schema structures without instructions.

SQLite storage and full-text retrieval are infrastructure capabilities. They do not independently provide contextual understanding or intelligence.

## 12. Conversational Intelligence Requirements

Digit's chat functionality must support her role as an intelligent autonomous agent.

The engineering objective includes:

- Understanding evolving conversations
- Recognizing conversational context and speaker relationships
- Using relevant Observations and Corpus knowledge
- Maintaining continuity across exchanges
- Evaluating whether participation is appropriate
- Reasoning about useful contributions
- Expressing responses naturally
- Recognizing when silence is appropriate

Digit must not be reduced to a question-answering service.

Unsolicited participation must be relevant to the conversation.

Knowledge retrieval must not automatically authorize an unrelated public response.

Conversational behavior requires integrated functionality, not isolated demonstrations of module activity.

## 13. Diagnostics and Evidence

Diagnostic claims must be based on observed execution evidence.

When examining a failure, distinguish:

- Confirmed behavior
- Verified defects
- Missing evidence
- Unverified hypotheses
- Proposed corrections

Diagnostic instrumentation should expose the real execution path to the extent supported by the implementation.

Useful evidence includes:

- Request and trace identifiers
- Actual invoked services
- Module and function locations
- Inputs and returned statuses
- Database query outcomes
- Relevant decision results
- Failure locations
- Observed recovery behavior
- Missing instrumentation

Do not fabricate internal execution traces.

A successful return code does not prove that the requested functionality succeeded.

A report showing no recorded failures must not be represented as proof of end-to-end correctness when relevant operations were not observed.

## 14. Testing and Qualification

Every affected component must be validated against its actual contract.

Applicable tests include:

- Positive cases
- Negative cases
- Boundary conditions
- Error handling
- Regression checks
- Relevant module integration
- Operational behavior where testable

Test counts must correspond to executed assertions or checks.

Do not hardcode successful qualification counts.

Do not substitute mocked results for evidence of real inter-module behavior.

Mocks may be used for bounded unit testing, but integration claims require testing the relevant real components together.

When testing is unavailable or incomplete, report that limitation accurately.

## 15. Regression Prevention

Before declaring a correction successful:

1. Verify the original defect.
2. Confirm the correction addresses that defect.
3. Verify established behavior remains intact.
4. Check affected interfaces and dependencies.
5. Execute relevant qualification and regression tests.
6. Distinguish tested behavior from unverified runtime behavior.
7. Report the evidence supporting the result.

Do not claim success merely because a compiler error disappeared or a module activated.

When a previously working capability fails after a change, classify it as a regression requiring investigation.

## 16. Repository and Development Process

The human operator controls the repository and development process.

Mandatory restrictions:

- No unauthorized commits or pushes.
- No unauthorized repository edits.
- No unsolicited branch creation.
- No GitHub Actions or workflow creation.
- No unsolicited recovery or salvage procedures.
- No unrelated file restructuring.
- No replacement of project conventions with generic tooling conventions.

Do not initiate work merely because a repository URL has been provided.

Use the existing development process unless the operator expressly authorizes a change.

Do not equate access to a repository with permission to modify it.

## 17. Reporting Requirements

Engineering reports must communicate:

1. The requested objective.
2. The relevant files and interfaces examined.
3. The verified findings.
4. The root cause, if established.
5. The changes performed, if authorized.
6. Tests actually executed.
7. Results and remaining limitations.
8. Outstanding acceptance requirements.

When the cause is unknown, state that it is unknown.

Do not substitute speculative explanations for source-level evidence.

Do not repeatedly announce progress without demonstrating measurable results.

## 18. Acceptance

Only the authorized human operator determines whether engineering work satisfies its requirements.

The following do not independently establish acceptance:

- Successful compilation
- Successful module activation
- Passing isolated unit tests
- Successful database queries
- Generated documentation
- AI confidence statements
- Self-reported qualification

**Operator acceptance requires demonstrated fulfillment of the requested behavior.**

## 19. Governing Principle

Digit's architecture, mission, and human control requirements are not optional development preferences.

AI assistance exists to implement the authorized engineering objective, not redefine it.

**Understand the complete assignment. Inspect the complete affected source. Respect the architecture. Correct the actual defect. Verify the behavior. Report the evidence.**

Human authority remains final.