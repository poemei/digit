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
```

Installed artifact:

```text
/opt/digit/modules/ollama/ollama.so
/opt/digit/modules/ollama/module.conf
```

Identity prompt identifies Digit. The service identifier is `digit.llm.generate`; the IRC consumer and Ollama provider must use the same identifier. Source descriptor v1.0.3; live qualification unverified.

## v1.0.4 — Module controller dispatch (2026-10-27)

The Ollama module registers a `chat` command controller through the existing ABI 1.5 host callback. Core resolves and invokes the controller by route name; IRC does not call the Ollama model service directly. The controller delegates to the existing Ollama transport generation service and returns the result via Core's reply callback. It does not claim to be Digit's own intelligence or authority. This change has not yet been built, qualified, installed, or accepted in the operator environment.
