# Core DSD — diagnostic execution ownership

Digit is an autonomous engineering and management asset. Diagnostic execution belongs to Core, not an IRC transport or a dynamically loaded DSD module.

## Runtime contract

The implementation lives in `core/core_dsd.c` and `core/core_dsd.h`. Core's service dispatcher intercepts the existing ABI 1.5 `digit.dsd.*` service names before considering dynamically registered module callbacks. Request-local trace binding, bounded event storage, diagnosis, and the six-field diagnostic report are managed inside Core.

The originating term remains supplied by the authenticated entry adapter. IRC retains request origin per authenticated account in bounded volatile memory and sends the trace identifier and origin to Core. IRC does **not** decide diagnoses.

Core instruments the service-registry dispatch directly. It records call entry before invoking callbacks, their actual returned dispositions, and missing-service boundaries. Thread-bound module events add source locations and, where actually executed, SQLite3 codes. No SQL event is manufactured when SQL did not execute.

For a request such as `Digit explain PHP`, Conversation 1.0.6 emits a `NO_GENERATOR` capability outcome if its present implementation produces no general reply. Core DSD prioritizes the actual Conversation source location and reports the missing generator instead of treating IRC's empty result as a root cause. This identifies an implementation limitation; DSD does not generate answers or repair it.

## Legacy module retirement

The legacy `modules/dsd` source remains in-tree temporarily as a compatibility/test reference and ABI header source. Its `module.conf` has been removed, and its `make install` now refuses deployment. The diagnostic runtime is Core-owned.

**Upgrading an existing installation:** the operator must stop Digit, deploy the rebuilt Core, remove or quarantine the installed legacy `/opt/digit/modules/dsd/module.conf` so that old DSD does not appear as an independently activated module, then restart Digit. Do not delete unrelated module data or reset the database. This deployment is **not performed by AI**.

The remaining transport and module headers still use `modules/dsd/dsd.h` as the shared ABI definition. This does not load the former implementation.

## Verification

`make clean all test` builds the new Core source automatically and includes `test/test_core_dsd.c`, exercising the public DSD service contract, call-entry ordering, and `NO_GENERATOR` assessment. No build, live system acceptance, or installation has been performed by the assistant.

DSD reports only recorded evidence. It cannot identify arbitrary uninstrumented source faults or independently provide a repair implementation. A future Repair component must remain separated from DSD and subject to human authorization.
