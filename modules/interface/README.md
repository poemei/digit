# Interface 0.1.1

Independent ABI 1.5 transport module. Listens on TCP port 8081 and accepts TLS handshakes using existing credentials:

- `/opt/digit/state/interface-cert.pem`
- `/opt/digit/state/interface-key.pem`

OpenSSL TLS 1.2 or newer; certificate/private key must both be readable by the Digit runtime account and match. The module never creates, modifies, or stores credentials.

**Transport only:** accepts TLS and closes the session. It does not parse application data, accept HTTP requests, route commands, or implement MCP, Slack, IRC, or Ask. Application protocols and routing require separately authorized integration; a successful TLS connection is not an application endpoint.

Failure to load credentials or bind port 8081 fails module activation. The listener stops cleanly on module deactivation. Single-threaded handshake processing, with a two-second socket timeout, limits capacity; no production concurrency claim is made.

Build: `make -C modules/interface clean && make -C modules/interface && make -C modules/interface test`.
Install: `sudo make -C modules/interface install`.
Operator test: restart Digit and inspect concise `modules.log` for `QUALIFICATION_GREEN` and `HOTLOAD_ACTIVE` for `interface`; then run `openssl s_client -connect 127.0.0.1:8081 -servername digit.stn-labz.com -brief` from the host. TLS handshake should succeed, then close without an HTTP response. Wrong/missing credentials or a port conflict must prevent activation.

On successful TLS listener startup the module emits `MODULE MESSAGE [INTERFACE] TLS listener active on port 8081; TLS required` through the Core-provided module message callback. Startup errors emit an `INTERFACE` failure message; the module does not claim success if the message cannot be recorded.
