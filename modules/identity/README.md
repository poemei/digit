# Digit Identity & Mission — 1.0.0

Native, standalone, read-only Digit module. The Director's approved operational mission is stored in `data/identity.conf` and installed in `/opt/digit/modules/identity/identity.conf`. The module reads the record at start and uses existing ABI command registration; it does not change Core, its Root identity, or the shared ABI.

## Supported commands

- `/identity mission` — exact Director-approved mission statement, without LLM generation.
- `/identity who` — read-only record of the operational identity.
- `/identity orders` — safe explicit notice that Core General Orders require an authorized read interface. No imitation or fabricated orders.

**Current architectural limit:** Without a Core-exposed read-only Root service, no independent module can retrieve Core's immutable General Orders under the established Core-mediated module interface. Duplicating Core orders into the module would create a competing authority. Core modifications require separate direct Director approval.

**Conversational route limit:** Core currently maps plain, unprefixed natural language to the registered `chat` controller. This module registers an explicit `/identity` controller. Questions such as `digicat: what is your mission?` will still reach Ollama unless a separately approved routing mechanism is implemented. Neither Core nor Ollama was modified here.

## Operator qualification

```sh
make -C modules/identity clean
make -C modules/identity
make -C modules/identity test
```

The native module's self-qualification executes ten model checks including duplicate-field negative validation; the standalone test also reads the approved record and checks the exact mission.

If accepted, the Director may install with `sudo make -C modules/identity install`. Core should perform standard qualification before activation. Verify with `digicat: /identity mission`, `digicat: /identity who`, and `digicat: /identity orders`.

No host build/test, activation, natural-language intent routing, or operator acceptance is claimed by this commit. Treat the `identity.conf` file as operator-controlled: changed mission text requires an explicit Director approval, and runtime changes require a module restart/reload to take effect.
