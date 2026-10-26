# Rictus IRC Transport Module

`irc` is Rictus's IRC transport/session module.

## Responsibility

The module owns IRC network/session behavior only:

- TLS client connection with certificate and hostname verification
- IRCv3 capability negotiation
- SASL PLAIN authentication over verified TLS
- nick/user registration
- channel join and rejoin after KICK
- PING/PONG handling
- bounded reconnect backoff after transport/session loss
- strict loading of `/opt/rictus/auth/irc/irc.env`

It does not implement conversational interpretation, reasoning, or mission logic.

## Required credential file

The module requires these exact keys in `/opt/rictus/auth/irc/irc.env`:

```text
RICTUS_IRC_SERVER=...
RICTUS_IRC_PORT=...
RICTUS_IRC_NICK=...
RICTUS_IRC_CHANNEL=...
RICTUS_IRC_ACCOUNT=...
RICTUS_IRC_PASSWORD=...
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
/opt/rictus/modules/irc/irc.so
```

Rictus watches `/opt/rictus/modules/`. A new or changed `/opt/rictus/modules/<module>/<module>.so` is snapshotted, loaded, verified, and qualified. A passing candidate is activated. A failing candidate remains inert while Rictus continues running; the same failed bytes are not retried until the file changes.


## Conversational handoff

IRC remains the transport owner. When the `rictus.llm.generate` service is
available, the IRC module forwards only:

- private messages sent directly to Rictus
- channel messages explicitly addressed as `rictus: ...`, `rictus, ...`, or
  `rictus ...`

Ordinary channel traffic is not sent to the LLM service. Generated CR/LF
characters are flattened before an IRC reply is queued.
