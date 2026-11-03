# Digit GitHub Read-Only Module — Cycle 1

**Version:** 1.0.0 source candidate (not yet qualified or deployed).

**Purpose:** Let Digit retrieve inspectable information from explicitly allowed public GitHub repositories using the existing native STN-Labz ABI 1.5. Read-only repository inspection is a prerequisite for future engineering assessments. It is not certification.

## Commands

Use the registered `/github` controller:

```text
/github poemei/digit
/github poemei/digit summary
/github poemei/digit tree
/github poemei/digit tree modules
/github poemei/digit file README.md
/github poemei/digit commits
/github poemei/digit issues
/github poemei/digit prs
```

`summary` reports repository metadata, root entries, recent commits, open issues and pull requests, with evidence links where available. `tree` lists a bounded page of directory entries. `file` returns file metadata, source URL, blob SHA and a bounded printable text excerpt. `commits`, `issues`, and `prs` return bounded first-page references. Responses indicate unavailable data rather than inventing findings.

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
