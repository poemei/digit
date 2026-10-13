# Digit IRC 0.1.0 — Rictus-compatible transport foundation

Linux C11 module using Digit ABI 1.5. Reference: stnlabz/rictus, especially src/irc.c, src/sasl.c, docs/CONFIGURATION.md. IRC/TLS/SASL remains entirely in this independent module. No changes to Core or Router.

Required runtime environment variables (provided to the digit systemd service, not committed to GitHub): DIGIT_IRC_SERVER, DIGIT_IRC_PORT, DIGIT_IRC_NICK, DIGIT_IRC_CHANNEL, DIGIT_IRC_ACCOUNT, DIGIT_IRC_PASSWORD. Use the **same server, port, and channel as Rictus**, but Digit's own nickname/account and password. TLS verifies server certificate and hostname. SASL PLAIN login is required. TLS credential payload is never logged.

The module starts a reconnecting IRC client, answers PING and JOINs after successful SASL and welcome numerics. Incoming IRC text is currently observational only, not an operator command path, and cannot grant Core authority. There is no outgoing chat relay or remote command dispatch in 0.1.0. SASL requires a server supporting IRCv3 SASL PLAIN. Configuration is mandatory: missing settings fail module activation, not a fallback to anonymous login.

Build: make -C modules/irc clean all test
Install after Core is installed: sudo make -C modules/irc install
Then restart Digit. Operator must supply config in systemd environment and perform end-to-end testing. Never put credentials in source control.

Known: this first version is a transport foundation; no IRC-to-Ask integration or IRC service registration. Qualification is descriptor/smoke only; TLS/SASL/channel behavior requires integration tests. No accepted status is implied by source commits.
