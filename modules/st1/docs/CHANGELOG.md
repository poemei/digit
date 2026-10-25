# ST-1 Research Module Changelog

## 0.1.0 — corrective source revision, 2026-10-25

- Corrected the originally incompatible module lifecycle. Digit's actual Core host provides registered ABI services but no command-registration callbacks; `st1` now registers/withdraws the read-only `st1.query` service without expecting unsupported callback pointers.
- Removed the falsely advertised `/st1` console command. It is not routable under the current Core host contract. No modifications were made to protected Core or Router.
- Added local `st1_manage list | watchlist | search TERM | show ID | history ID` read operations alongside operator-controlled `upsert`.
- Preserved append-only evidence revisions, classification, impact assessment, provenance and mission watchlist.
- Updated the test fixture for the actual service-only host; added isolated CLI persistence and negative-case tests, without touching `/opt/digit`.
- Integration into Digit's conversational controller remains a separate unresolved route contract; do not label this candidate end-to-end accepted or deploy it without operator review.

## 0.1.0 — original candidate, 2026-10-25

- Initial native C ST-1 evidence registry for poemei/digit.
- Source-attributed entries, revisions and read-only ABI query contract.
- Original CLI-only write path; automated ingestion, Slack reporting and flight requirement changes excluded.
- Initial advertised console route later found incompatible and explicitly withdrawn above.
