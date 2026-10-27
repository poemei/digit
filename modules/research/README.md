# Digit Research Module — 1.0.0

Digit ABI 1.5 module. This is **not** an ST-1 Mission Module. No Slack or external network research providers. Core unchanged.

## Scope
Security, autonomous systems, software engineering, and space systems.

## Architecture
- **Router:** existing Core command registry maps `research` to the module.
- **Controller:** `src/research_module.c` owns request dispatch and view responses.
- **Model:** `src/research_model.c` reads approved-location local text sources, without changing their contents.
- **View:** command callback renders a cited excerpt, explicitly labeled UNVERIFIED.
- **Database:** not used yet; no substitute intelligence or approval authority is created.

## Evidence input
The operator may place UTF-8 `.txt` research materials in:
- `/opt/digit/research/sources/security/`
- `/opt/digit/research/sources/autonomous-systems/`
- `/opt/digit/research/sources/software-engineering/`
- `/opt/digit/research/sources/space-systems/`

The module searches for literal text, not semantic similarity. The first matching source excerpt is returned as UNVERIFIED. It is **not** authorized Digit knowledge. Filenames are used for source citation. Nothing automatically trains or modifies Digit.

## Build and test
```sh
make -C modules/research clean all test
sudo make -C modules/research install
```

## Console acceptance
Once the operator observes version 1.0.0 active, test:
```text
/research status
/research topics
/research security firmware
/research mission test
```
Expected: status/topics, source-backed or UNKNOWN, then usage guidance. Exact command prefix depends on active console routing; observe the registered command.

## Known limitations
This initial module is a safe, functional **local research retrieval boundary**, not autonomous online research. Live web collection, scheduling, source qualification, deduplication, structured research database, and ingestion approval require operator-defined source and authorization requirements. No claim that this module performs those activities.

No actual server build, Core admission, hotload, or end-to-end acceptance is established by committing files.
