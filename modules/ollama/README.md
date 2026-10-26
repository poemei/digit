# Rictus Ollama LLM Module

Rictus-specific local LLM transport.

- Endpoint: `http://127.0.0.1:11434/api/generate`
- Model: `llama3.2:1b`
- Service: `rictus.llm.generate`
- No credential/config file is required.
- Ollama owns generation only; it does not own Rictus state, authority, IRC, or intelligence.

Build and test:

```sh
make clean all test
sudo make install
```

Installed artifact:

```text
/opt/rictus/modules/ollama/ollama.so
/opt/rictus/modules/ollama/module.conf
```
