# Digit IRC transport — Libera.Chat

**Version:** 0.1.0 (unqualified, inactive)

This is an ABI 1.5 shared-object transport module. It reads configuration from
`/opt/digit/state/auth/irc.env` and restricts the server to `irc.libera.chat`
and the channel to `#stn-labz`. The existing configuration keys
`DIGIT_IRC_SERVER`, `DIGIT_IRC_PORT`, `DIGIT_IRC_NICK`,
`DIGIT_IRC_CHANNEL`, `DIGIT_IRC_ACCOUNT`, and
`DIGIT_IRC_PASSWORD` are preserved. TLS certificate and hostname checks
are enabled, with SASL PLAIN authentication inside TLS.

**Boundary:** The transport passes a fixed-size `irc_input_t` request to the
host ABI service `digit.message`; it does not invoke Core or MVC sources
directly. Its response is an opaque UTF-8 message of at most 511 bytes;
responses containing CR/LF are rejected. The host must supply a matching,
authorized service implementation and routing policy separately. Nicknames
are not assumed to be authenticated account names.

**Critical restrictions:** The module intentionally reports qualification
failure because its mandatory 20 real qualification tests are not yet
implemented. The binary MUST NOT be activated until independent qualification
evidence, Core-side service routing, and operator authorization exist.
No IRC connection is established by merely building or discovering it.

## Build, test, and install

From `modules/irc/`, the module independently supports:

```sh
make all
make test
make clean
sudo make install
```

`make install` places `module.conf` and `bin/irc.so` in
`/opt/digit/modules/irc/`, with the shared object in its `bin/` child.
The module's `test/` folder contains its own ABI descriptor smoke test;
**this is not the mandatory 20-test qualification suite**.

From the Digit repository root, use `make`, `make test`, and
`sudo make install`. Installation does not confer authorization,
qualification, or module activation.


## Observation boundary diagnostics — v0.3.0

Every successfully parsed IRC channel Observation request now records its returned service status with syslog under `digit.irc observation_service`. The log contains only the integer status and whether the Observation service accepted the request; it does not include user accounts, channel messages or message text. Acceptance is **not** proof of database persistence. Missing services and failed dispatch are no longer silently discarded, and offline qualification covers success, missing service, invalid state, malformed requests, and unexpected response size.

Operator diagnosis after activating IRC v0.3.0: `journalctl --since "10 minutes ago" --no-pager | grep 'digit.irc observation_service'`. If no entries appear after a channel message, investigate IRC channel parsing/routing. If the service rejects calls, inspect the separately installed Observation v0.1.3 module. If it accepts but no rows are present, inspect the Observation-to-SQLite3 forwarding contract. No Core or database changes are introduced.

IRC's previous 0.2.21 is a pre-existing historical version. Under the approved version numbering doctrine, this engineering release advances to **0.3.0** rather than extending a revision above 10.
