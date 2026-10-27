# Digit Ollama LLM Module

Digit-specific local LLM transport.

- Endpoint: `http://127.0.0.1:11434/api/generate`
- Model: `llama3.2:1b`
- Service: `digit.llm.generate`
- Registered Core controller route: `chat`
- No credential/config file is required.
- Ollama owns generation only; it does not own Digit state, authority, IRC, or intelligence.

Build and test:

```sh
make clean all test
sudo make install
sha256sum build/ollama.so /opt/digit/modules/ollama/ollama.so
stat -c '%U:%G %a %n' /opt/digit/modules/ollama/ollama.so
```

Installed artifact:

```text
/opt/digit/modules/ollama/ollama.so
/opt/digit/modules/ollama/module.conf
```

Identity prompt identifies Digit. The service identifier is `digit.llm.generate`; the IRC consumer and Ollama provider must use the same identifier. Source descriptor v1.0.4; live qualification unverified.

## Installation integrity and ownership (2026-10-27)

The Makefile requires an existing `digit` user and `digit` group (overridable via `MODULE_USER` and `MODULE_GROUP`). Its `install` target stages the built shared object within the module directory, verifies byte-for-byte equality, atomically renames it into the hotload path, and verifies the installed bytes and ownership `digit:digit`. It fails on mismatch rather than silently leaving an unverified installation. The installed file is mode 0755; `module.conf` is mode 0644 with the same owner/group.

The operator reported on 2026-10-27 that the installed Ollama shared object was root-owned, dated 2026-10-26, and had a different SHA-256 than the newly built artifact. The last observed running Ollama module version was 1.0.3; the 1.0.4 activation remains unverified. Run installation as a privileged operator and confirm a new activation receipt. Ownership alone did not prove or explain the outdated bytes.

## v1.0.4 — Module controller dispatch (2026-10-27)

The Ollama module registers a `chat` command controller through the existing ABI 1.5 host callback. Core resolves and invokes the controller by route name; IRC does not call the Ollama model service directly. The controller delegates to the existing Ollama transport generation service and returns the result via Core's reply callback. It does not claim to be Digit's own intelligence or authority. This change has not yet been built, qualified, installed, or accepted in the operator environment.
