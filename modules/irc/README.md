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


## Observation service rejection recovery (IRC v0.3.1)

Runtime journal evidence showed the independently deployed `digit.observation.record` v0.1.3 returning ABI `STNLABZ_MODULE_ERR_INVALID_ARGUMENT` (numeric 1) for channel messages. The Observation implementation source is absent from the repository, preventing correction of that independent service here.

IRC continues to call the Observation service **first**. On that specific invalid-argument outcome only, IRC forwards the **unchanged** `digit_observation_record_t` request to the registered `digit.sqlite3.observation_insert` service. SQLite3 owns that contract and makes the Observation plus `utterance.observed` context insert atomically in v0.4.7. No fallback occurs after successful primary acceptance, an unknown error, or any other error status. No Core, database schema, or Observation module changes occur.

The journal logs `observation_route=sqlite3_direct` when fallback is attempted, then logs the final `observation_service status=<number> accepted=<0|1>`. A successful service result is not a substitute for validating actual rows. This is a **bounded compatibility recovery**, not an implementation-level repair of the unavailable Observation module.

Qualification now covers primary success without fallback and the invalid-argument fallback dispatch, alongside earlier failure cases. Build and activate IRC v0.3.1 under operator control. Verify via a newly observed channel message and its `observations.event_id` joined to `context.source_id`. If fallback fails, its final status remains visible and requires further investigation.


## Direct fallback completion diagnosis (v0.3.2)

IRC logs `observation_route=sqlite3_direct completed=1 status=... response_bytes=...` **after** the fallback service returns. If only the earlier `reason=observation_invalid_argument` entry appears, completion has not been observed. This release adds instrumentation without changing the existing fallback decision.


## Verified SQLite receipt (v0.3.3)

On the existing `digit.observation.record` invalid-argument fallback, IRC now requires a complete SQLite3 `digit_observation_receipt_t` with positive Observation/context row IDs and a NUL-terminated, nonempty event ID. A service return of OK without a valid receipt is treated as invalid state, not logged as persistence verified. The mock qualification tests valid and malformed receipt paths. Activate SQLite3 0.4.9 before IRC 0.3.3.


## Developer diagnosis output (v0.3.4)

Authenticated private `diagnose my last request` now emits a structured evidence-based diagnosis, distinct from `show trace` (which continues to print the retained raw event trail). Sections: Originating Term, Route, Module, Code, SQL, and Assessment. The previous private request is retained only in a bounded account-scoped in-memory IRC slot; it is not submitted as text to generic DSD trace events and is not persisted in SQLite. Control characters are excluded and the display is capped at 240 characters. Existing trace ownership and private-message authorization are preserved.

A missing SQLite3 trace event is reported as **not observed**, not as a successful, failed, or skipped SQL statement. No database query is invented and no inference is presented as a captured downstream trace. The latest source location names the observed boundary, not an unobserved fault location. The DSD module's raw trace ABI is unchanged; this change is in the IRC diagnostic presentation layer. Production activation and request-level acceptance remain operator-controlled.
