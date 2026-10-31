# Digit Live Research — native module v1.2.0

The native C module provides a Core-mediated `digit.research.search.v1` service and `/research <question>` command. **No paid service, search subscription, or API key is required.** It does not change Core, Router, IRC, or module ABI 1.5.

## Answer contract

Research happens internally. The human receives the requested answer, in plain English. It does not expose URLs, search snippets, timestamps of retrieval, or commentary about how research works.

For `when is the next SpaceX rocket launch?`, Research uses The Space Devs Launch Library 2 upcoming-launch JSON endpoint, filters strictly for SpaceX, disregards past launches and completed/failed flights, chooses the earliest future target, and returns the mission, rocket, launch site, Pacific local date/time, and schedule status. It uses **live response data**, not a hardcoded launch date. Example format:

`Next SpaceX launch: Starlink Group 15-18. Rocket: Falcon 9 Block 5. Launch site: Vandenberg SFB. Target: Oct 30, 2026 at 9:02 PM Pacific. Status: Go for Launch.`

The example is a fixed regression fixture, **not** an assertion about the current launch at runtime. If the structured feed fails, the module gives a brief inability-to-verify response instead of inventing an answer.

Other time-sensitive questions use a keyless HTTPS public RSS search source, retaining semantic subject checks but returning short headline/summary leads rather than URLs. **Those leads are not authoritative answers.** Further subject-specific extraction is necessary for reliable factual answers beyond the supported structured launch query.

## Boundaries

- Read-only HTTPS requests to fixed public destinations only; TLS validation enabled, no redirects or proxies.
- Bounded response size (512 KiB), timeout (10 seconds), twelve research requests per minute, query-hash auditing without plaintext queries.
- A simple deterministic harmful-intent gate exists but is **not** a complete legal/safety classifier. Broader untrusted public access requires additional safeguards.
- The public Launch Library 2 free API has provider rate limits, schedule uncertainty, and availability constraints. A reported target time is a *scheduled* time, not proof the launch occurred or is guaranteed.
- Caller validates service status and returns the result without sending untrusted source content to Ollama.

## Build and operator acceptance

```sh
git pull
make -C modules/research clean all test
make -C modules/ollama clean all test
sudo make -C modules/research install
sudo make -C modules/ollama install
```

Modules should be installed and activated under operator control and tested through the actual Digit client. Verify a SpaceX launch question produces a complete single plain-English response, and a missing/unavailable research service responds briefly without hallucinating. Test ordinary chat and an unrelated live query for regressions.

**Source committed; build, module qualification, live provider availability, activation, and end-to-end acceptance are not independently verified.**
