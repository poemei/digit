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
