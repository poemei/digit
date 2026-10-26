# Digit IRC Transport Module

`irc` is Digit's IRC transport/session module.

## Responsibility

The module owns IRC network/session behavior only:

- TLS client connection with certificate and hostname verification
- IRCv3 capability negotiation
- SASL PLAIN authentication over verified TLS
- nick/user registration
- one or two comma-separated channels, each joined and tracked; targeted rejoin after KICK
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
DIGIT_IRC_CHANNEL=#stn-labz,#second-channel
DIGIT_IRC_ACCOUNT=...
DIGIT_IRC_PASSWORD=...
```

Unknown keys, duplicate keys, missing keys, invalid ports, invalid nick/account values, and invalid channel values cause module start to fail. `DIGIT_IRC_CHANNEL` accepts one or two distinct channel names separated by a single comma, without spaces. Credentials are not written to module output.

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

A reply to an addressed channel message goes to the originating channel. Explicit generic channel-send service calls continue using the first configured channel. Startup is considered online only once all configured channels have joined; rejoin-after-KICK is handled per channel. Ordinary channel traffic is not sent to the LLM service. Generated CR/LF
characters are flattened before an IRC reply is queued.

## v1.0.8 — IRC startup failure diagnostics (2026-10-26)

IRC startup now records safe, categorized server numeric failures (nickname conflicts, server bans, SASL errors, and channel admission failures), a server ERROR indicator with its untrusted text withheld, and socket poll/TLS read failures. Raw IRC messages, SASL payloads, credentials, and account passwords are never logged by these diagnostics. The existing 30-second authenticated-channel startup gate and reconnect policy remain unchanged. Qualification and live server admission are not yet verified after this source update.

## v1.0.9 — Two-channel IRC configuration (2026-10-26)

Use `DIGIT_IRC_CHANNEL=#stn-labz,#another-channel` to join two channels; the single-channel form remains supported. There must be no whitespace between names. Each channel is tracked separately for join completion and KICK recovery. Addressed replies return to their source channel, while the generic module channel-deliver service retains the first channel as its default. The existing startup gate requires both channels to be joined. Build, qualification, and live dual-channel behavior are not yet verified.
