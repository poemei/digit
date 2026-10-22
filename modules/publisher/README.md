# Digit Publisher v0.1.0 — native ABI 1.5 Markdown publishing

Service: `digit.publisher.publish`. No Core changes, no LLM dependency.

The caller submits `publish_request_t` with `site` (`chaos` or `stnlabz`), `kind` (`announcements`, `news`, `page`, `posts`), category, title and Markdown source. The handler posts authenticated JSON to the configured HTTPS endpoint using an independently scoped Bearer credential per site. The server must explicitly return HTTP 200/201 with JSON `{"published":true,"id":"...","url":"https://..."}`. Only then is publication reported as successful.

**Integration boundary:** This is a server-side API contract **required by the module**, not a verified claim about the currently deployed ChAoS MVC sites. The existing admin pages and a human login do not prove that a compatible API exists. No website request is made when its endpoint is disabled. Wire this to a real authenticated ChAoS MVC publishing route after inspecting that route. Publishing scopes remain enforced by the website.

The transport does not independently decide editorial relevance: Digit's existing evidence/reasoning modules must supply approved publication requests. Autonomous discovery, editorial selection, duplicate tracking, and publication scheduler are NOT yet implemented and must not be treated as working merely because this module qualifies.

Credentials must never be committed. The config file must be owned by root or the Digit process UID, ordinary 0600 file. Both domains can be configured independently.

Build and qualify: `make -C modules/publisher clean all test`; install only after operator acceptance.
