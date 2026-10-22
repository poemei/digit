# Digit Publisher v0.2.0 — ABI 1.5

Publisher loads `/opt/digit/auth/sites/sites.json`, containing up to three independent sites named `chaos`, `stnlabz`, and `poemei`. The JSON must be owned by root or the Digit process UID, be a regular file, and have no group or other permission bits (use chmod 600). See `sites.json.example`.

Each site includes `enabled`, `endpoint`, `token`, `kinds`, and optional `voice`. Disabled sites require no credentials or endpoints. Enabled sites require a HTTPS endpoint on the exact configured site's domain, a Bearer token, and at least one authorized publishing kind. The `kinds` list is a local limitation, not proof of remote permission. The `voice` metadata is reserved for Digit's editorial decision-maker; the transport does not itself write prose.

The existing ABI service `digit.publisher.publish` accepts the same fixed-size request struct with site, kind, category, title, and raw GFM Markdown. A request denied by local `kinds` never reaches a website. The transport submits JSON with kind, category, title, content, and format. A publication is confirmed only if the server returns HTTP 200/201 and JSON containing `published:true`, `id`, and HTTPS `url`.

**Important:** This submission JSON shape is still a required, unverified server contract, not a claim that ChAoS MVC currently provides that endpoint. Site-specific publishing route and credential integration require inspecting the deployed module controllers. This module is not autonomous editorial planning, scheduling, or duplicate detection. No live posts have been demonstrated.

Test: `make -C modules/publisher clean all test`. No Core modifications.

## Verify the existing ChAoS MVC login first

The independent read-only acceptance program is `test_site_login.c`. It follows the verified ChAoS MVC form: GET `/login`, read the hidden `csrf_token`, POST `username`, `password`, and `csrf_token` to `/auth/login`, and fetch `/admin` with the same session cookies. It checks the authenticated admin dashboard; no content is created or changed.

The program reads a local file at `/opt/digit/auth/sites/login.conf` containing two lines: `username=...` and `password=...`. Only the Digit account or root should own the file; mode must be 0600. This file is not stored in GitHub. The same values are reused for all three domains, each with a separate cookie session. A failure is reported as NOT VERIFIED rather than assumed to be an invalid password.

Run from the Digit checkout: `make -C modules/publisher test-login`. This performs real login attempts and is separate from offline module qualification. The result is **unverified** until executed on Digit's host with her account credentials. The earlier Bearer publishing transport is still unverified and must not be used for publication.
