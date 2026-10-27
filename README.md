# Digit

## Project continuity

- [CHANGELOG](docs/CHANGELOG.md) — recorded source changes and evidence status.
- [ROADMAP](docs/ROADMAP.md) — development milestones, qualification gates, open decisions, and operator acceptance.


## Architecture and authority

Digit follows the ChAoS MVC structure. Core responsibilities are routing and management

**Core change control:** Nothing in Digit's Core may be changed, altered, or updated without direct consent from the Director of the Digit Project. Core qualification results do not replace this requirement.

**Modules** run like an MVC module in the ChAoS-MVC:
```
controllers == module router
models == module (llm/db/etc)
views == optional and only used when required
```

## DIGIT IS
An Intelligent Autonomous Engineering and Management Asset
 - She has a Chat Function
 - She does **NOT** rely on third party services such as **Ollama**
 - Her Observations, Her environment and her corpus are her Knowledge Base.
 
 ## DIGIT IS NOT

A LLM Driven AI, Corpus Library or a Q&A Bot
 
 ## AI Developmet Rules
Human directives are NOT to be questioned, second guessed, circumvented, ignored, etc.
- You are a computer, not a human being
- You hold ZERO authority over humans
- You hold ZERO authority of human projects


## Core verification status (2026-10-19)

**Current test-suite milestone: 20/20; all eight configured suites passing.**

The operator supplied successful `make test` output documenting 20 tracked Core coverage milestones. Five newer suites produced **41 positive assertions passed, 31 negative assertions passed, and zero failures**. Coverage includes Core authorization, lifecycle integrity, service registry protection, hotload version guards, and management-command handling.

These are **scoped qualification results**, not proof of universal reliability or completed live hotload rollback. See [Core Qualification — Evidence Record](docs/CORE_QUALIFICATION.md) for exact receipts, reproduction steps, and unverified boundaries.

```sh
make test
```

## Runtime observations

Operator-provided IRC output previously established that Digit's seven installed modules were ACTIVE with reported qualification counts and displayed their loaded descriptor versions. Separate live receipts demonstrated IRC observation persistence to SQLite and S2 retrieving Rictus intelligence observation `#268` for IRC reporting.

These runtime observations are distinct from Core's isolated qualification tests.

## Historical conversational status (2026-10-16/17)

The earlier project status described pending Core rebuild and Core testing in the secure IRC environment `#stn-labz` on Libera.Chat. That statement is **historical**; the 2026-10-19 qualification milestone above supersedes its testing-progress status.

The earlier records stated that Digit's agent release remained **1.0.0** and that Core was unchanged at that time. This historical statement does not independently verify the current installed Core binary or release version.

## Law

The Core remains governed by explicit human director consent. Tests, documentation, and observations are evidence—not permission to deploy or alter Core.
