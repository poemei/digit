# Digit Publisher v0.4.1 — ABI 1.5

The production service `digit.publisher.publish` now uses the tested ChAoS MVC browser-form contract, not an invented Bearer JSON API. It accepts a revised fixed-size request (`site`, `kind`, `category`, `title`, `markdown`, `subject`, `source_actor`, `evidence_ref`, `post_title`, `post_markdown`) and responds with confirmation/error in the original response shape. Configured sites: `chaos`, `stnlabz`, `poemei`. PoeMei publishing uses the existing IRC configuration and requires the caller to supply the authenticated IRC account-tag identity. Supported kinds: `announcements`, `news`, `page`, `posts`. SRT and Intelligence are not enabled.

## Credentials and permissions

The process reads `/opt/digit/auth/sites/login.conf` for `username=...` and `password=...`, and `/opt/digit/auth/sites/sites.json` for allowed destinations and `kinds`. Both must be ordinary files owned by root or Digit's UID, with permissions 0600. An enabled site's configured `kinds` gate controls whether requests can proceed. `endpoint` and `token` from older JSON configurations are no longer consulted. **Existing sites.json configurations with `enabled:false` remain disabled**: update that local file to enable allowed sites. Copy the shape from `sites.json.example`, preserving the private file ownership/mode. Do not put credentials in Git.

## Publishing contract

For each call the Publisher performs fresh GET `/login`, CSRF-protected POST `/auth/login`, accesses the authenticated admin form, submits the kind-specific form, verifies the published result, POSTs `/logout` with CSRF, and checks the prior session cannot access `/admin`. It maintains cookies only in the request-local libcurl session.

- **Announcements:** `/admin/announcements`, `action=create`, `body`, `published=1`; verifies its admin listing. Announcement bodies' public Markdown rendering depends on the separately installed Announcements module.
- **Core Posts:** `/admin/posts`, `body`, `published=on`, `featured_image_id` chosen by matching `digit.png` in the existing Media dropdown. Refuses a missing or mismatched image. Verifies the public `/posts/{slug}` includes an image. The Posts slug computation preserves the Core controller's hyphen behavior.
- **Core Pages:** `/admin/page`, `action=create`, `slug`, `description`, `status=published`, `content_file=main.md`, `content`. Verifies the public `/{slug}` page.
- **News:** `/admin/news`, `action=create`, `slug`, `title`, `category`, `summary`, `body_markdown`, `status=published`. Verifies public `/news/{slug}`.

The Publisher refuses an existing title/slug found in its admin view; it does not overwrite posts. **No automatic IRC ingestion, editorial writing, scheduling, authorization or external intelligence gathering is implemented here.** Calls must originate from an authorized service consumer. This is transport only.

## Qualification and deployment

Offline ABI qualification: `make -C modules/publisher clean all test`. This does not make live publication requests. The existing standalone acceptance tests previously demonstrated the corresponding website form operations across three domains. **The updated production service itself still requires a build, qualification, and runtime integration test on Digit's host; source commit is not proof it is operational.**

Do not run the hardcoded live acceptance targets unless a new public test entry is intended. No changes to ChAoS MVC Core are required by this version.

## Editorial gate (v0.4.0)

`subject` must be `chaos_mvc` for `chaos`, and `stnlabz` for `stnlabz`. An evidence reference is required, but Publisher cannot independently validate the semantic relevance or veracity of arbitrary source content; Digit's trusted editorial selector must do so before invocation. **Do not treat caller-provided subject/evidence text as independent proof.**

For `poemei`, `subject=poemei_quote`, `source_actor` must match `DIGIT_IRC_MOMMY_ACCOUNT` from the **existing** `/opt/digit/state/auth/irc.env` (as used by Digit IRC) and `evidence_ref` must be nonempty. Publisher requires this file to be owned by root or the Digit UID with mode 0600, just like its other private configuration. This is an account equality gate, **not independent verification that the caller actually obtained its source_actor from an authenticated IRC account-tag**. The trusted caller must convey genuine account-tag provenance and preserve the original statement; otherwise a fabricated claim can still pass the string check.

Announcements require **both** `post_title` and `post_markdown`. Publisher first submits and verifies a same-domain explanatory Core Post (using the existing `digit.png` Media selection) and only then submits the Announcement. This ordering prioritizes preventing orphan Announcements: the Post may appear before its Announcement. If Announcement fails after the Post succeeds, the Post remains published, and the caller receives failure; there is **no rollback**. Optional deep explanatory Pages remain separately invoked and are not automatically inferred or generated. Other publication kinds must leave `post_title` and `post_markdown` empty.

**ABI compatibility:** The request struct is larger in v0.4.0; service consumers built against v0.3.0 must update their request layouts. Calls with the old size are rejected. The service name remains `digit.publisher.publish`. This policy gate is not a replacement for independently authenticated origin, evidence validation, or authorization.

Run offline qualification with `make -C modules/publisher clean all test` before deployment. Do not run live acceptance test targets without intent to publish public material.

**v0.4.1 deployment:** `git pull`, then `make -C modules/publisher clean all test`. Confirm file permissions for `/opt/digit/state/auth/irc.env` and restart/reload the module using Digit’s existing module lifecycle. A passed qualification does not prove live content attribution or site publication.
