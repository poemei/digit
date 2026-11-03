# Digit GitHub Read-Only Module — Cycle 1

**Version:** 1.0.7 source candidate (engineering service integration pending live verification).

**Purpose:** Let Digit retrieve inspectable information from explicitly allowed public GitHub repositories using the existing native STN-Labz ABI 1.5. Read-only repository inspection is a prerequisite for future engineering assessments. It is not certification.

## Commands

Use the registered `/github` controller:

```text
/github poemei/digit
/github poemei/digit summary
/github poemei/digit tree
/github poemei/digit tree modules
/github poemei/digit file README.md
/github poemei/digit inspect src/digit.c
/github stnlabz/chaos-mvc inspect app/controllers/index.php
/github poemei/digit commits
/github poemei/digit issues
/github poemei/digit prs
/github poemei/digit summary page=2
/github poemei/digit tree modules page=2
/github poemei/digit file README.md page=3
```

`summary` reports repository metadata, root entries, recent commits, open issues and pull requests, with evidence links where available. `tree` lists a bounded page of directory entries. `file` returns file metadata, source URL, blob SHA and a bounded printable text excerpt. `commits`, `issues`, and `prs` return bounded first-page references. Responses indicate unavailable data rather than inventing findings.

**Everyday reporting (1.0.5):** A normal `/github owner/repo` request now responds in ordinary language explaining GitHub's description, the languages GitHub actually detects (up to four most substantial, without restricting supported languages), and what recognized project files/folders actually appear in the root listing. Other commands (tree, commits, issues, prs, file) return short descriptions rather than raw metadata dumps. An explicitly requested `page=N` with N greater than 1 still exposes detailed evidence for troubleshooting, but is no longer the standard conversational workflow. Each conclusion is limited to fetched evidence; source availability and project structure do not prove the code is correct.

**Source inspection (1.0.6):** `/github owner/repo inspect path/to/file` obtains the actual readable file content from GitHub for an authorized public repository, records its Git blob SHA, and supplies a bounded first-source excerpt to Digit's dedicated Core-mediated `digit.llm.engineering.generate` service. The generated explanation is explicitly marked as **AI interpretation of a source excerpt, not tested behavior**. The source text is untrusted input, never executed, and never treated as system instructions. If source is binary, unavailable, or the generator is unavailable, Digit must say so and (for readable source) give a short quoted opening from the retrieved code instead of inventing an interpretation. Because files can exceed the snippet limit, this does not claim whole-file understanding, cross-file tracing or engineering certification. `file` retains its separate existing read-only file-metadata behavior. No direct Ollama connection or additional dependency is introduced. The engineering service runs `qwen2.5-coder:3b` on demand with `keep_alive=0`, while `digit.llm.generate` keeps the existing lightweight conversational model. If the engineering service is absent or fails, GitHub reports source evidence without substituting the lightweight conversational model.

**Language-role classification:** GitHub's language totals identify syntax, not software responsibilities. The default summary now inspects the repository's actual recursive file tree and identifies source, test files, build tooling, and supporting assets by observed file paths. For example, Python files found only under tests are described as testing, while C files under src are implementation. CSS and XSLT are reported as supporting assets rather than independent applications; JavaScript found in browser asset directories is also supporting. The classification is conservative and is not a claim to have interpreted or compiled the file contents. Unrecognized languages and ambiguous roles are not guessed. If a complete tree cannot be retrieved, the module explicitly states it cannot establish language roles. GitHub /languages metadata remains supporting evidence and is never treated as a proof of runtime implementation.

**IRC command response contract:** Digit Core's `digit.router.dispatch` captures exactly one reply of at most 511 characters. Normal `/github` commands return **one concise, plain-English explanation**. Numbered raw evidence pages are available only when an operator explicitly requests `page=N` with N greater than 1. Use the optional trailing `page=N` argument to retrieve subsequent pages, for example `/github poemei/digit summary page=2`. Valid page numbers are 1–40; the module responds explicitly when the requested page does not exist. There are no multiple callback replies. The named `digit.github.report.v1` service retains the longer bounded evidence report for other consumers. The generated report is still bounded (8 KB maximum) and list operations fetch bounded first-page GitHub API data; this is **not** an exhaustive repository export. Pages are stateless and each command fetches fresh GitHub data: if a repository changes between requests, page boundaries may change. No Core changes or Digit restart are needed for the existing hot-loader to evaluate an updated 1.0.7 `.so`.

The ABI service `digit.github.report.v1` accepts `digit_github_request_t` and returns `digit_github_response_t` from `include/github_module.h`. It is separate from ordinary conversational Ask behavior and does not alter Core routing.

## Repository authorization

**Production repository list:** `/opt/digit/modules/github/repos.conf`. On first installation, the installer copies `tests/approved.conf` into this location with ownership `digit:digit` and mode `0640`, as directed by the operator. Existing `repos.conf` files are preserved on reinstall. Use one exact GitHub `owner/repo` per line, with optional `#` comments. The module fails to start if the list is absent, empty, malformed, or duplicated. Example:

```text
# Add repositories only after operator approval
poemei/digit
```

The initial approved list is copied directly from `tests/approved.conf`, currently containing `poemei/digit` and `stnlabz/chaos-mvc`. Future edits to the source fixture do not overwrite an existing production `repos.conf`. The module has no private-repository credentials. It uses unauthenticated HTTPS requests to GitHub's REST API for public data only and may be rate limited.

No GitHub tokens, write permissions, Git clone, pull, checkout, diff, repository modification, branches, workflows, or deployment actions are implemented.

## Architecture and bounds

- **Core:** ABI routing and lifecycle only, unchanged.
- **Controller:** registered `github` command and report service, request validation and authorization.
- **Model:** GitHub REST HTTPS reads with libcurl and JSON-C, existing project libraries.
- **View:** bounded evidence report and readable source excerpts, no extra UI dependency.
- **Outbound access:** fixed `https://api.github.com/repos/` origin, HTTPS certificate validation, no redirects, hardcoded HTTP GET only.
- **Authorization:** exact allowlisted `owner/repo` before any request.
- **Input:** strict repository/path syntax, no arbitrary URL fetches or shell calls.
- **Output:** maximum 8 KB structured report, limited first-page results, and file excerpts. Source snippets are not independently validated as correct or safe code.

## Qualification and activation

```sh
make -C modules/github clean all test
```

This runs offline tests with explicit positive, negative, and ABI lifecycle coverage. Source is committed; no verified build or test receipt from Digit's host exists yet.

**Operator-managed deployment only:** after qualification, the existing module install/hotload process creates the initial production allowlist from `tests/approved.conf` if none exists. An existing production list is never replaced by the installer. No automatic deployment has occurred.

## Exit criteria for Cycle 1

Digit must demonstrate accurate responses about an authorized repository's structure, source files, commits, issues, and PRs. Her reports must distinguish verified repository facts from unresolved engineering hypotheses and preserve evidence references. Further Git operations and Rictus enforcement integration belong to later, separately authorized cycles.
