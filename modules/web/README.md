# Digit Web Transport 1.0.1 — Outbound HTTP

This replaces the unauthorized Unix socket implementation of 1.0.0. Digit initiates outbound HTTP communication to an existing local web server, like IRC initiates its own network connection. Digit does not expose a new listening port or Unix socket.

## Integration contract for ChAoS MVC

The **visitor interface** is `/chat`. The ChAoS MVC module separately implements a **server-only** transport operation at `/chat/transport`. This operation must not be publicly accessible or included in the chat view.

Digit polls with an HTTP POST and `Content-Type: application/json`:

```json
{"type":"poll"}
```

The ChAoS module returns HTTP 204 when no message is pending, or HTTP 200 with exactly one queued request:

```json
{"id":"request-001","message":"Hello","display_name":"Mei Shaw"}
```

Digit passes `message` to the existing Core-mediated `digit.router.dispatch` service, using the authenticated website account’s `display_name` as the router sender, then POSTs the result back to the same operation. The existing `digit-web` sender is retained for legacy queue records without the optional `display_name` field. The transport does not assert a privileged router `account` identity or send PHP sessions:

```json
{"type":"reply","id":"request-001","status":"ok","message":"Hello!"}
```

The website acknowledges with HTTP 200. `status` is `ok` or `error`. HTTP replies are bounded and messages are correlated by `id` (1–64 ASCII letters, digits, underscores or hyphens). The PHP module must own request queueing, correlation, delivery acknowledgment and idempotency if a reply is retried. This Digit transport does not implement persistent conversations or transport-message de-duplication.

**Default local endpoint:** `http://127.0.0.1/chat/transport`. An optional `DIGIT_WEB_TRANSPORT_URL` environment variable can select another path under `http://127.0.0.1/chat/transport`, but cannot change host, scheme or port. Configure the existing web-server localhost virtual host so this route reaches the correct ChAoS MVC installation. No browser-side credentials, socket paths, ports or internal endpoints should be shown.

**Security boundary:** The ChAoS MVC server must restrict the transport operation to loopback-origin requests and independently authorize each browser-side user before accepting messages for delivery. Digit currently has no individual browser-user authorization; the authenticated website display name is supplied as a conversational sender label, but no Digit account authorization is asserted. The name is trusted only to the extent that the PHP queue and local transport endpoint are restricted. Do not expose the transport operation to internet clients. Restricting to 127.0.0.1 alone does not defend against untrusted processes already on the host.

## Existing Core contract

The HTTP transport sends the same router request type used by IRC: `digit_route_request_t`, invoking `digit.router.dispatch`. The existing router accepts at most 2047 bytes of input and supplies up to 511 text bytes of reply. The new transport removes IRC framing, but **cannot** change these Core limits. No Core edits or ABI changes were made.

## Lifecycle and verification

The module starts an outbound HTTP polling worker; it does **not** require a socket directory, Unix permissions, a PHP UID setting, a new listening port or an existing HTTP route to initialize. If the ChAoS endpoint does not exist yet, the worker remains active but cannot exchange messages. It does not falsely report web chat as operational.

```sh
git pull
make -C modules/web clean all test
```

Only after native qualification succeeds may the operator choose `sudo make -C modules/web install` and activate/hotload it. Installation and production activation remain human-controlled. Any old systemd override such as `DIGIT_WEB_ALLOWED_UID` is obsolete for this version and may be reviewed or removed by the operator; this module does not require it.

## Limits

One HTTP job per poll; one worker; no streaming, browser authentication, queue persistence, or guaranteed delivery semantics in Digit's transport. The PHP module is a separate development effort. Runtime connectivity to the real ChAoS MVC endpoint has not been verified by offline C tests.
