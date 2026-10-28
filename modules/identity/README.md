# Digit Identity & Mission — 1.1.1

Director-authorized mission, identity and General Order responses are now owned by the standalone Identity module's installed data file, `/opt/digit/modules/identity/identity.conf`.

- `mission`, `name`, `organization`, `role`, and `order1`/`order2`/`order3` are all required; invalid or incomplete data causes module startup to fail.
- `/identity mission`, `/identity who`, `/identity orders`, `/identity first general order`, `/identity second general order`, `/identity third general order` return data-backed, deterministic responses.
- Natural-language questions like `digicat, what is your mission?`, `digicat, who are you?`, and `digicat, what is your 1st/2nd/3rd general order?` route through the existing Ollama chat controller to `digit.identity.answer` via Core's existing service registry; matching questions never reach LLM generation.
- The previously added Core service `digit.root.orders.read` has been **removed**. Protected Root identity and its own governing orders remain unchanged inside Core; the module holds operator-approved conversational answer copies. Keep the copies synchronized by explicit Director approval if governing orders ever change.
- IRC and the shared ABI are unchanged. No module-to-module direct binding.
- Module record is read at startup. For already installed systems, install the updated record as well as the new identity binary; simply replacing the binary is insufficient.

## Operator checks

```sh
make clean && make && make test
make -C modules/identity clean && make -C modules/identity && make -C modules/identity test
```

Rebuild/install under operator control and reactivate Identity v1.1.1. Ollama v1.0.5 can remain unchanged. Test `/identity first general order` and `what is your first general order?`, plus second and third; confirm unrelated chat is still routed to Ollama.

Source committed; local build, actual deployment, qualification, and runtime acceptance have not been verified here.
