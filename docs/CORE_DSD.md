# Core DSD — diagnostic execution ownership

Digit is an autonomous engineering and management asset. Diagnostic execution belongs to Core, not an IRC transport or a dynamically loaded DSD module.

## Runtime contract

The implementation lives in `core/core_dsd.c` and `core/core_dsd.h`. Core's service dispatcher intercepts the existing ABI 1.5 `digit.dsd.*` service names before considering dynamically registered module callbacks. Request-local trace binding, bounded event storage, diagnosis, and the six-field diagnostic report are managed inside Core.

The originating term remains supplied by the authenticated entry adapter. IRC retains request origin per authenticated account in bounded volatile memory and sends the trace identifier and origin to Core. IRC does **not** decide diagnoses.

Core instruments the service-registry dispatch directly. It records call entry before invoking callbacks, their actual returned dispositions, and missing-service boundaries. Thread-bound module events add source locations and, where actually executed, SQLite3 codes. No SQL event is manufactured when SQL did not execute.

For a request such as `Digit explain PHP`, Conversation 1.0.6 emits a `NO_GENERATOR` capability outcome if its present implementation produces no general reply. Core DSD prioritizes the actual Conversation source location and reports the missing generator instead of treating IRC's empty result as a root cause. This identifies an implementation limitation; DSD does not generate answers or repair it.

## Legacy module retirement

The legacy `modules/dsd` directory has been removed from the repository. The shared diagnostic contract is `core/stnlabz_abi/dsd.h`. The diagnostic runtime is Core-owned.

**Upgrading an existing installation:** the operator must stop Digit, deploy the rebuilt Core, remove or quarantine the installed legacy `/opt/digit/modules/dsd/module.conf` so that old DSD does not appear as an independently activated module, then restart Digit. Do not delete unrelated module data or reset the database. This deployment is **not performed by AI**.

The transport and other modules include `core/stnlabz_abi/dsd.h` through their existing compiler include path. This does not load a diagnostic module.

## Verification

`make clean all test` builds the new Core source automatically and includes `test/test_core_dsd.c`, exercising the public DSD service contract, call-entry ordering, and `NO_GENERATOR` assessment. No build, live system acceptance, or installation has been performed by the assistant.

DSD reports only recorded evidence. It cannot identify arbitrary uninstrumented source faults or independently provide a repair implementation. A future Repair component must remain separated from DSD and subject to human authorization.

## Core registry-versus-handler fault attribution (2026-10-24)

Core now distinguishes two observably different failure boundaries that previously shared the misleading status `SERVICE_NOT_FOUND`:

- `SERVICE_NOT_FOUND`: Core's registry lookup did **not** find a registered, available handler. The DSD event names the attempted service and the Core registry lookup location.
- `HANDLER_NOT_FOUND`: Core **did** find and invoke a registered handler, but that handler returned `STNLABZ_MODULE_ERR_NOT_FOUND`. The DSD event names the service and callback result. This does **not** establish that the handler's dependencies were unregistered; deeper cause remains unverified unless a nested trace event identifies it.

The Core report's `Code:` line includes `service=<service_name>` alongside module, location and status. Report selection favors an instrumented module's explicit failure over a generic Core wrapper with otherwise equal severity, while retaining Core registry errors when those are the actual failure. A successful user-visible reply can coexist with a failed optional service; the report identifies the observed failure rather than claiming the entire request failed.

The regression in `test/test_core_dsd.c` exercises both distinct cases. These source-level changes do not change the module ABI, dynamically load DSD, inspect private SQL, or automatically repair anything. Before declaring a root cause, follow nested service events where available; if no deeper event exists, report that its origin is unverified. Operator-controlled Core compilation, tests, and deployment are required before live DSD output changes.

### Observed IRC family-probe fall-through

The October 24 trace for `explain yourself` included `digit.socials.family` before Conversation and `digit.gru.generate`. Source inspection established the call chain: `modules/irc/irc.c` attempts the optional family responder; `modules/socials/socials.c` returns `STNLABZ_MODULE_ERR_NOT_FOUND` when `family_intent()` finds no family-specific intent; IRC then continues routing. This return does **not** mean `digit.socials.family` is unregistered, and is not in itself a request failure.

Core DSD now ranks a registered handler's `HANDLER_NOT_FOUND` below a later recorded successful response, while retaining the unmatched handler event in the complete trace. A genuine Core registry lookup miss (`SERVICE_NOT_FOUND`) remains high priority. A handler no-match with **no** downstream success remains reportable rather than being erased. The regression test reproduces the optional family probe followed by successful `digit.gru.generate` generation and verifies that the final response is not falsely assessed as missing registration.

This fixes attribution for the observed source path, **not** the GRU's weak generated content. Meaningful answer quality still requires separate language qualification.

### Skipped retrieval is not a failed retrieval (2026-10-24)

An IRC request such as `explain yourself` contains no eligible factual Corpus search subject after Conversation filters grammatical and discourse words. Consequently **no SQLite corpus search service is invoked**. Previous Conversation code emitted `RETRIEVAL_UNAVAILABLE` whenever no search succeeded, even if no search was attempted. Reasoning independently interpreted `retrieval_attempted=0` as a failure; Core DSD then reported this synthetic technical defect.

The correction preserves separate observations: `RETRIEVAL_SKIPPED` for no attempted Corpus query, `NO_CORPUS_MATCH` when a query succeeds without relevant evidence, and `RETRIEVAL_UNAVAILABLE` when at least one actual query is attempted but none succeeds. Reasoning now returns `ABSTAIN` for no attempted query, while retaining `FAILURE` when an attempted retrieval fails; its DSD event names an abstention explicitly. Core DSD treats skipped/no-subject/abstention events as low-priority information, not failures outranking a successful generated reply.

The recorded lack of a SQLite operation therefore represents a non-attempt, **not an established SQLite failure**. This fixes attribution only. It does not qualify the GRU's language quality or authorize new SQL queries. A Core regression test reproduces the full skipped-search → Reasoning abstention → successful GRU response path. Operator-run compilation, qualification and live traces remain required.

### Reasoning hotload version correction (2026-10-24)

Reasoning's `retrieval_attempted=0` abstention fix and `REASONING_ABSTAIN` DSD event were committed earlier, but the Reasoning module descriptor was left at v0.1.8. Core's `core_hotload.c` denies a changed candidate unless the candidate descriptor version is greater than the active incumbent. Reasoning has now been incremented to **v0.1.9**, with no ABI changes, so the operator can build and install a properly versioned candidate for qualification and hotload. Source synchronization (`git pull`) alone cannot activate a revised `.so`; module compilation and deployment remain operator-controlled. Confirm the actual installed Reasoning incumbent before assuming automatic replacement; do not bypass version gates.
