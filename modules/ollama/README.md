# Digit Ollama LLM Module

Digit-specific local LLM transport.

- Endpoint: `http://127.0.0.1:11434/api/generate`
- Model: `llama3.2:1b`
- Service: `rictus.llm.generate`
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

Identity prompt now identifies Digit while retaining the legacy `rictus.llm.generate` ABI to preserve compatibility with the transplanted IRC module. Source descriptor v1.0.3; live qualification unverified.
