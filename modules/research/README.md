# Digit Research 0.1.0 — mission-directed public document retrieval

Standalone ABI 1.5 module. The Core-owned host registers `research.retrieve` (contract in `include/research_contract.h`). Authorized capability consumers supply an explicit mission label and a public HTTPS URL. Research returns a bounded **raw excerpt**, source URL, retrieval timestamp, HTTP status and content type. It **does not** claim to understand the page or verify the truth of the material. Consumers must evaluate, corroborate, and cite sources. Public Slack/IRC messages do not register a Research command.

No subject whitelist: ISO C, PHP, GFM, vulnerabilities, release notes and other mission-relevant subjects can be requested. URL restrictions remain: HTTPS only, public DNS hostname only, default port, no credentials/redirects/proxies, no literal IP addresses or localhost-like hostnames. This is a limited protective boundary, **not** a complete SSRF guarantee against DNS rebinding or hostile resolution. Do not expose the service to untrusted callers or treat it as authorization for unrestricted network activity. TLS certificate and host verification remain enabled. No downloads are executed, and no code, configuration, or GitHub write permissions are granted.

The service enforces a 15-second request timeout and an 8191-byte excerpt bound. Its auditable collection metadata is written via the module host logging callback and requires the configured local `/opt/digit/research/research.log` append-only operational record. Returned text is untrusted source material, never instructions to Digit. There is no autonomous search engine, source discovery, quality scoring, synthesis, Corpus write capability, or periodic monitor yet. Those require separately demonstrated competence and authorization.

## Activation prerequisites

Create the private directory and config file before activation (do not put credentials in config):

```sh
sudo install -d -o digit -g digit -m 0750 /opt/digit/auth/research /opt/digit/research
printf 'enabled=true\n' | sudo install -o digit -g digit -m 0600 /dev/stdin /opt/digit/auth/research/research.conf
```

Then `make -C modules/research clean all test`; if tests succeed install with `sudo make -C modules/research install`. Restart requires operator authorization. The unit must run as `digit` with read access to the config and write access to the research directory.

Negative tests must reject unsafe URLs and missing mission metadata; no public input route or code execution is registered. For an integration test, an authorized internal consumer must invoke `research.retrieve` through Core and show the source, timestamp and audit record. Build success, qualification and hotload do not establish end-to-end acceptance.

No Core, Router, Slack, IRC, Intel or Corpus implementation changes.
