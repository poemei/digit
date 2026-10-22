# Digit Publisher v0.3.0 — ABI 1.5

The production service `digit.publisher.publish` now uses the tested ChAoS MVC browser-form contract, not an invented Bearer JSON API. It accepts the original fixed-size request (`site`, `kind`, `category`, `title`, `markdown`) and responds with confirmation/error in the original response shape. Supported sites: `chaos`, `stnlabz`, `poemei`. Supported kinds: `announcements`, `news`, `page`, `posts`. SRT and Intelligence are not enabled.

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
