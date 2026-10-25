# Digit ST-1 Research Registry v0.1.0

**Source candidate. Not installed, activated, qualified by running Digit, or accepted by the operator.**

Native C ABI 1.5 module for the **poemei/digit** architecture. Registers the independent `/st1` command and read-only `st1.query` service through Core's existing command/service registry, with no changes to Core or Router. Source path: `modules/st1/`. The separate operator-controlled `st1_manage` program writes an append-only, revisioned registry. There are no bundled research assertions or automated ingestion.

Build and tests from the repository root: `make -C modules/st1 test`. That compiles against `core/stnlabz_abi/module.h`; do not use the unrelated `stnlabz/digit` ABI tree. No installation occurs in the test target.

Query through the normal Digit console after separately authorized installation and activation: `/st1 list`, `/st1 watchlist`, `/st1 search relay`, `/st1 show ST1-1`, `/st1 history ST1-1`.

Authorized local-write example, **format only, not an approved evidence record**:
```sh
/opt/digit/modules/st1/bin/st1_manage upsert ST1-1 MISSION "Relay demonstration" "https://example.org/report" "2026-10-25" "Published milestone" Reported Medium communications None "Published demonstration"
```

The schema requires ID, kind (PAPER, REPORT, STANDARD, MISSION), title, HTTPS source, publication date, summary, evidence (Verified, Reported, Inferred, Unknown), relevance (High, Medium, Low), affected systems, impact (Reinforces, Challenges, PotentialChange, None), and milestone (required for MISSION). Reusing an ID appends a new revision. Read access returns at most eight records per request; large collections require a later operator-approved pagination feature. Physical data file: `/opt/digit/research/st1/research.tsv` (private, single-link, append-only under authorized CLI). Existing content must be attributable; the module does not establish that a source is verified merely because a user classified it Verified.

**Authorization:** No unrestricted write command or writable ABI service. Core ABI service calls alone do not establish caller authority; filesystem permissions and explicit local operator action govern writes. No Core, Router, Agent, GRU, source corpus, mission requirement, or deployment changes. Research findings cannot lift ST-1 planning hold or automatically change approved requirements.

**Integration limit:** Explicit `/st1` route and ABI lookup are provided. Free-form questions through Dispatcher/Intent/Corpus/Validator are not automatically connected to the new service. That would require a separately approved integration contract.

Operator qualification must distinguish source, compilation/tests, module admission, activation, console behavior, and explicit acceptance. Required negative checks include rejected malformed records, missing registry, absent IDs, and blocked unauthorized writes. **AWAITING OPERATOR END-TO-END TESTING AND ACCEPTANCE.**
