# Digit Publisher v0.4.7 — ABI 1.5

The production service `digit.publisher.publish` now uses the tested ChAoS MVC browser-form contract, not an invented Bearer JSON API. It accepts a revised fixed-size request (`site`, `kind`, `category`, `title`, `markdown`, `subject`, `source_actor`, `evidence_ref`, `post_title`, `post_markdown`) and responds with confirmation/error in the original response shape. Configured sites: `chaos`, `stnlabz`, `poemei`. PoeMei publishing uses the existing IRC configuration and requires the caller to supply the authenticated IRC account-tag identity. Supported kinds: `announcements`, `news`, `page`, `posts`, and opt-in `intelligence` for STN-LABZ only. SRT remains disabled.

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

For `poemei`, `subject=poemei_quote`, `source_actor` must match `DIGIT_IRC_MOMMY_ACCOUNT` from the **existing** `/opt/digit/state/auth/irc.env` (as used by Digit IRC) and `evidence_ref` must be nonempty. Publisher will use this file if it is readable and securely owned with mode 0600. If it is unavailable, Publisher still activates, but poemei.com publication remains denied. This is an account equality gate, **not independent verification that the caller actually obtained its source_actor from an authenticated IRC account-tag**. The trusted caller must convey genuine account-tag provenance and preserve the original statement; otherwise a fabricated claim can still pass the string check.

Announcements require **both** `post_title` and `post_markdown`. Publisher first submits and verifies a same-domain explanatory Core Post (using the existing `digit.png` Media selection) and only then submits the Announcement. This ordering prioritizes preventing orphan Announcements: the Post may appear before its Announcement. If Announcement fails after the Post succeeds, the Post remains published, and the caller receives failure; there is **no rollback**. Optional deep explanatory Pages remain separately invoked and are not automatically inferred or generated. Other publication kinds must leave `post_title` and `post_markdown` empty.

**ABI compatibility:** The request struct is larger in v0.4.0; service consumers built against v0.3.0 must update their request layouts. Calls with the old size are rejected. The service name remains `digit.publisher.publish`. This policy gate is not a replacement for independently authenticated origin, evidence validation, or authorization.

Run offline qualification with `make -C modules/publisher clean all test` before deployment. Do not run live acceptance test targets without intent to publish public material.

**v0.4.1 deployment:** `git pull`, then `make -C modules/publisher clean all test`. Confirm file permissions for `/opt/digit/state/auth/irc.env` and restart/reload the module using Digit’s existing module lifecycle. A passed qualification does not prove live content attribution or site publication.

**v0.4.2 regression fix:** IRC identity lookup is no longer a mandatory module startup dependency. Missing/unreadable IRC identity configuration blocks only PoeMei-attributed publishing; it does not break the existing ChAoS MVC or STN-LABZ Publisher startup. This preserves the other authorization gates.

## STN-LABZ Intelligence publishing (v0.4.3)

The uploaded `intelligence_digit_publish` ChAoS module exposes a level-7 CSRF-protected `/admin/intelligence` form using `action=create_intelligence`, `title`, `content`, `source`, `status=ACTIVE`. Publisher now supports `kind=intelligence` through the existing browser-login workflow and verifies the title on the authenticated administrator page and public `/intelligence` index before returning success. This is not a direct JSON API and doesn't change Core or IRC. The module allocates its own `INT-...` record identifier. Publisher currently returns the verified index URL, not an individual record permalink; it uses `verified-index` as a verification token rather than inventing an INT ID.

This kind remains **fail closed** until explicitly added to the deployed `/opt/digit/auth/sites/sites.json` `stnlabz` kinds list; the checked-in `sites.json.example` illustrates the opt-in. The existing `subject=stnlabz` and evidence_ref checks still apply. **Do not put private S2 Slack intelligence into the public Intelligence repository without a separate publication authorization.** Only intended public reports should enter this route.

Review finding: the uploaded Intelligence controller's JSON write endpoints `report`, `observation`, and `assignment` do not show authentication or CSRF checks. This Publisher integration **does not use** those endpoints; their current external callers and authentication boundary must be reviewed before treating them as safe. This is source integration, not a successful live acceptance test.

## v0.4.4 — Existing SAI-DSA Core Page update

New opt-in `kind=page_update` is restricted to `site=stnlabz`, the already existing Core Page slug `sai-dsa`, and an authenticated `/admin/page?edit=sai-dsa` form. It uses the Core controller's `action=update` and `original_slug=sai-dsa` contract rather than creating a page or using the separate Intelligence module. The page's title and description are taken from the authenticated edit form; its published Markdown format must already be selected. The operation fails closed if those conditions are not met. It verifies the expected S2 snapshot marker on the public page after submission. The update is intentionally a *full-body replacement* of that Core Page, not a section-level merge. Preserve/back up any existing editorial content before opting in. A post-submit verification failure may leave the edited page in place; there is no atomic rollback.

The deployed `/opt/digit/auth/sites/sites.json` must explicitly opt in `page_update` for `stnlabz`. Existing kinds remain available (capacity now six); `sites.json.example` shows the intended configuration. All other destinations are denied this operation. Module loader version is 0.4.4. No private S2 Slack message is forwarded via Publisher.

## v0.4.5 — Fail-closed startup recovery

Publisher activation no longer depends on the private site configuration or login credential files being readable and valid at module startup. A missing/invalid configuration instead leaves publishing disabled while the service itself can register, allowing module activation. Any attempted publication in that state is denied by `credentials_ready`; this change does not bypass the existing destination, subject, kind or identity gates. Startup writes a non-secret diagnostic to stderr for configuration failure or service registration failure. If service registration itself fails, module activation still fails.

A successful loader activation confirms module lifecycle only: `/sai-dsa` publication remains blocked until configuration and credentials pass their usual checks. Deploy the configuration before activation; the module reads it at startup and does not silently enable publishing later. No Core or production secret changes.

## v0.4.6 — Existing STN-LABZ Core Page authorization

Digit S2's `page_update` operation on the fixed existing `sai-dsa` Core Page is now authorized by STN-LABZ's existing `page` publication kind. The caller must still pass the `site=stnlabz`, `source_actor=digit.s2`, and `subject=stnlabz` editorial checks; the target remains fixed and authenticated Core Page update verification is still required. This avoids a private sites.json configuration migration when Core Page publishing was already authorized. Explicit `page_update` remains supported. The module version changes to 0.4.6, required for hotload qualification. This does not establish live website publication or override invalid credentials.

## v0.4.7 — Core Page content-file compatibility

Removed the former requirement that existing `/sai-dsa` must already use `main.md`. ChAoS MVC Core Pages `action=update` supports changing the content file from `main.html` or `main.txt` to `main.md`; the S2 Publisher always submits Markdown and preserves the existing slug, title, description, and `published` state. The update still requires authenticated Core administration, passes the S2 editorial gates, and verifies the expected snapshot marker on the public page before returning success. This is a full-body replacement, so the old content should be backed up before activating automated publication. Failure to verify the public marker returns a specific error. No deployment or live update is claimed from this commit.
