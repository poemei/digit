# Digit IRC Transport Module

`irc` is Digit's IRC transport/session module.

## Responsibility

The module owns IRC network/session behavior only:

- TLS client connection with certificate and hostname verification
- IRCv3 capability negotiation
- SASL PLAIN authentication over verified TLS
- nick/user registration
- channel join and rejoin after KICK
- PING/PONG handling
- bounded reconnect backoff after transport/session loss
- strict loading of `/opt/digit/auth/irc/irc.env`

It does not implement conversational interpretation, reasoning, or mission logic.

## Required credential file

The module requires these exact keys in `/opt/digit/auth/irc/irc.env`:

```text
DIGIT_IRC_SERVER=...
DIGIT_IRC_PORT=...
DIGIT_IRC_NICK=...
DIGIT_IRC_CHANNEL=...
DIGIT_IRC_ACCOUNT=...
DIGIT_IRC_PASSWORD=...
```

Unknown keys, duplicate keys, missing keys, invalid ports, invalid nick/account values, and invalid channel values cause module start to fail. Credentials are not written to module output.

## Build and test

```sh
make
make test
```

The module qualification callback runs the same protocol self-test set used by `make test`. Qualification does not perform live network access.

## Install

```sh
sudo make install
```

The installed hotload artifact is:

```text
/opt/digit/modules/irc/irc.so
```

Digit watches `/opt/digit/modules/`. A new or changed `/opt/digit/modules/<module>/<module>.so` is snapshotted, loaded, verified, and qualified. A passing candidate is activated. A failing candidate remains inert while Digit continues running; the same failed bytes are not retried until the file changes.


## Conversational handoff

IRC remains the transport owner. When the `rictus.llm.generate` (legacy ABI service name) service is
available, the IRC module forwards only:

- private messages sent directly to Digit
- channel messages explicitly addressed as `digit: ...`, `digit, ...`, or
  `digit ...`

Ordinary channel traffic is not sent to the LLM service. Generated CR/LF
characters are flattened before an IRC reply is queued.

## v1.0.8 — IRC startup failure diagnostics (2026-10-26)

IRC startup now records safe, categorized server numeric failures (nickname conflicts, server bans, SASL errors, and channel admission failures), a server ERROR indicator with its untrusted text withheld, and socket poll/TLS read failures. Raw IRC messages, SASL payloads, credentials, and account passwords are never logged by these diagnostics. The existing 30-second authenticated-channel startup gate and reconnect policy remain unchanged. Qualification and live server admission are not yet verified after this source update.
