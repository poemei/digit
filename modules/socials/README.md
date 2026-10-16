# Digit Socials — 0.1.0

Independent, nonverbal emoji selection module using the existing ABI 1.5 service registration.

The only module authorized to own social-expression resources and selection. This module contains Unicode emoji resources only: **no fixed language, greetings, nicknames, scripts or dialogue templates**.

Service: `socials.select`. Contract: `include/socials.h`. An already-authorized caller supplies a category (or ANY) and a nonempty, NUL-terminated contextual key. Selection is deterministic for identical inputs. The response contains one Unicode emoji and its selected category; the caller decides whether to request a symbol, and whether to use the result. Socials does not initiate conversation, decide whom to address, interpret Corpus knowledge, or send messages.

There is **no automatic integration into Ask, IRC, Slack, Reasoning, or Interpretation** in 0.1.0. Selection is a lower-level capability, not proof of autonomous contextual understanding. No Core, Router, or ABI changes.

## Build and verify

```sh
make -C modules/socials clean all test
```

After engineering checks, install separately with `sudo make -C modules/socials install`. Qualification, activation and operator end-to-end acceptance are separate gates.

The caller must supply a meaningfully differentiated contextual key; this service makes no claim to interpret the semantics of the key.
