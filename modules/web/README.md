# Digit Web Transport — 1.0.0 (source baseline, not activated)

Native-C, local-only communications between Digit and a future ChAoS MVC PHP module at /digit-chat.

## Boundaries

- Uses a Unix domain socket, not a TCP listener or IRC. Location: `/run/digit-web/digit.sock` (private server configuration only).
- Calls the existing Core-mediated `digit.router.dispatch` service, using the same transport-neutral ingress as IRC.
- Refuses startup unless `DIGIT_WEB_ALLOWED_UID` is configured with the trusted local PHP process UID and a protected runtime directory exists.
- Verifies callers with Linux `SO_PEERCRED`; socket mode 0660 and directory permissions add filesystem isolation.
- Does not authenticate browser visitors, implement website sessions or expose a public web route. PHP process identity is not visitor authorization.
- Does not alter Digit Core, its ABI, IRC, Ask, Research, Ollama or GitHub.
- Existing router ABI limits replies to 511 text bytes. Removing IRC does not remove this internal contract. Larger replies will require an authorized extension or a larger existing service interface.

## Wire contract v1

Request: unsigned big-endian 32-bit length (1–2047), followed by that many UTF-8 bytes (no NUL, CR, LF, control bytes). The message is the normal Digit input text, e.g. `Hello` or `research PHP 8.5`.

Response: one status byte (`0` = Digit replied, `1` = error), followed by unsigned big-endian 32-bit payload length, followed by exactly that many bytes (maximum 511). One request per connection; synchronous and serial processing, no stream or retained conversation history.

PHP model implementation: connect to `unix:///run/digit-web/digit.sock` with `stream_socket_client`; send `pack('N', strlen($message)) . $message`, handling partial writes; read exactly five bytes, decode status and length with `unpack`, then read exactly the reply length, handling partial reads. Validate status/length, application sessions, web authorization and CSRF before forwarding. Never expose the socket path, user IDs, credentials or runtime details to the browser.

## Runtime provisioning (operator-controlled)

Provision `/run/digit-web` with ownership by the Digit runtime user, with no world access or group-write permission. A dedicated, setgid local group can allow the specific PHP process to traverse the directory and use the socket, while the module still checks its exact UID. Do not run PHP as Digit or grant general privileged groups for convenience. The `/run` directory is ephemeral and needs operator-controlled boot provisioning. Module startup fails closed if the directory is absent or socket path is already occupied; stale sockets require human inspection.

Set `DIGIT_WEB_ALLOWED_UID` in Digit's process environment to the trusted PHP runtime UID before operator-controlled module activation. Do not expose this server-side configuration publicly.

## Socket ACL behavior

At listener creation, the module sets an explicit minimal access ACL on the socket (owner read/write, owning group read/write, everyone else denied), replacing inherited default ACL entries that may silently remove group write access. Failure to set this ACL prevents listener startup. The runtime directory retains its operator-controlled permissions. The PHP worker must have socket directory traversal and belong to the socket's owning group; the existing `SO_PEERCRED` allowed-UID check remains independent and enforced. The permission setup is recreated automatically whenever Digit binds a new socket.

Building requires POSIX ACL development headers and library (`libacl1-dev` on Ubuntu). The module's test target includes an ACL regression test.

## Build/qualification gate

```sh
git pull
make -C modules/web clean all test
```

Only after qualification passes, the operator may install using `sudo make -C modules/web install`. Installation alone is not module activation. This module has **not** been compiled, qualified, installed or activated on Digit's VPS during this source commit.

## Deferred

Public-facing ChAoS MVC module, website authentication/authorization, end-user identity handling, response sizes exceeding existing Core router limits, streaming, history and concurrent sessions are not implemented in this increment. No website access is safe to enable before user-level authorization exists.
