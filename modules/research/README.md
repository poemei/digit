# Digit Live Research — v1.3.0

Research owns the interpretation, safety screening, evidence collection and construction of responses for **live research requests**. It does not own the general `chat` route, IRC transport, Ollama generation, or protected Core.

## Registered capabilities

- `/research <question>`: explicitly requested research.
- `digit.research.search.v1`: existing explicit search service (preserved).
- `digit.research.answer.v1`: new conversational research boundary, invoked by Ask via Core ABI 1.5.

The answer service returns `RESEARCH_NOT_APPLICABLE` for an ordinary non-research conversation. Ask can then invoke an appropriate different model. When Research claims a question, it either produces an evidence-grounded answer or a brief inability-to-verify/denied status. **It never passes a claimed question to Ollama for invented knowledge.** Classification rules are entirely within the Research module; Ask contains no research keywords or search logic.

## Live-source behavior

For `when is the next SpaceX rocket launch?`, Research queries an unauthenticated HTTPS Launch Library 2 upcoming-launch data source, validates that the operator is SpaceX, excludes past/completed launches, and formats the next mission, rocket, location, target Pacific time and schedule status. Source content and provider endpoint do not appear in the response. This requires live access to the provider and is not a hardcoded launch timetable.

Other time-sensitive questions are currently not fully supported by structured evidence and are **declined briefly**, rather than returning raw RSS headlines or guessing. Explicit `/research` remains available for source discovery, but its generic RSS results are unverified leads and are not a substitute for a factual answer.

Search uses fixed HTTPS destinations, no proxies or redirects, TLS verification, 512 KiB bounded responses, 10-second timeout, a 12-queries-per-minute limit, and hashed query disposition logging without plaintext query logging. The local safety gate denies certain actionable harmful requests, including before ordinary chat fallback. This is not comprehensive content-policy classification.

No paid provider, credentials or API key required. The public structured launch data source has availability/rate limitations. Dates and launch states may change.

## Validation and install

```sh
git pull
make -C modules/research clean all test
make -C modules/ollama clean all test
make -C modules/ask clean all test
sudo make -C modules/research install
sudo make -C modules/ollama install
sudo make -C modules/ask install
```

Operator must verify the actual activated versions and final routed behavior, not just file copies. Ollama v1.1.0 relinquishes `chat`; Ask v1.0.0 then owns it. Research remains independent, with its `/research` route and named services. Core and IRC are unchanged.

**State: source implemented, compilation/qualification/activation/operator acceptance unverified.**
