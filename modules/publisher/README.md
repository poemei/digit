# Digit Publisher v0.2.0 — ABI 1.5

Publisher loads `/opt/digit/auth/sites/sites.json`, containing up to three independent sites named `chaos`, `stnlabz`, and `poemei`. The JSON must be owned by root or the Digit process UID, be a regular file, and have no group or other permission bits (use chmod 600). See `sites.json.example`.

Each site includes `enabled`, `endpoint`, `token`, `kinds`, and optional `voice`. Disabled sites require no credentials or endpoints. Enabled sites require a HTTPS endpoint on the exact configured site's domain, a Bearer token, and at least one authorized publishing kind. The `kinds` list is a local limitation, not proof of remote permission. The `voice` metadata is reserved for Digit's editorial decision-maker; the transport does not itself write prose.

The existing ABI service `digit.publisher.publish` accepts the same fixed-size request struct with site, kind, category, title, and raw GFM Markdown. A request denied by local `kinds` never reaches a website. The transport submits JSON with kind, category, title, content, and format. A publication is confirmed only if the server returns HTTP 200/201 and JSON containing `published:true`, `id`, and HTTPS `url`.

**Important:** This submission JSON shape is still a required, unverified server contract, not a claim that ChAoS MVC currently provides that endpoint. Site-specific publishing route and credential integration require inspecting the deployed module controllers. This module is not autonomous editorial planning, scheduling, or duplicate detection. No live posts have been demonstrated.

Test: `make -C modules/publisher clean all test`. No Core modifications.
