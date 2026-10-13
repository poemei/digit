# Interface 0.1.3

Independent ABI 1.5 TLS transport module on TCP 8081 using existing credentials at `/opt/digit/state/auth/interface-cert.pem` and `/opt/digit/state/auth/interface-key.pem`. TLS 1.2 minimum; certificate validation remains with the HTTPS client.

This revision accepts only bounded `POST /ask` with `Content-Length` 1–512 and `text/plain` UTF-8 body. It calls the existing `router.dispatch` service with `core_authorized=0`. The Router explicitly permits anonymous requests **only** to the module-owned `route./ask` service. No other route gains anonymous authority; the Interface cannot execute commands, interpret intent, or access SA functions. Responses are `{"answer":"..."}` JSON. No authentication UI or token is used. A client should not treat transport success as a grant for other actions.

Ask 0.1.0 currently provides bounded responses and a truthful not-yet-supported answer for general natural-language requests; **this is not an LLM or general intent engine**. No persistent history is provided. Connection handling is single threaded, with a three-second per-client socket timeout. No concurrency claims.

Build: `make -C modules/interface clean all test` and `make -C modules/ask clean all test`. Install both modules along with the Core host changes, restart Digit, and issue `curl -i https://digit.stn-labz.com:8081/ask -H 'Content-Type: text/plain' --data-binary 'hello'`. Expect HTTP 200 and a JSON answer, once the operator confirms installation and qualification.
