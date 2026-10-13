# Code Reference 0.1.0

Independent ABI 1.5 module. Provides language-neutral service
`code_reference.analyze` with versioned payload in `include/code_reference.h`.

Initial backend: ISO C11 compiler frontend invoked with syntax-only checks
(no shell and no execution of supplied source). Requires a C compiler named
`cc` on the running host; operator may configure `CODE_REFERENCE_CC`.
The request supplies a bounded translation unit, not a filesystem path.
No source tree changes, diagnostics-only. No Core changes or DSD integration.

Status ANALYZED means syntax/compiler checks passed, **not** understanding
program intent or absence of defects. Runtime, concurrency, lifetime,
correctness, and root-cause analysis remain future work. Unsupported
languages (C++, C#, Go, PHP, SQL/MySQL, HTML, CSS) return an explicit
unsupported status rather than fabricated findings.

Build and test independently from this directory:
`make clean && make && make test`.

Human/operator acceptance remains required before further development.
