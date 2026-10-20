# Digit Socials — ABI 1.5

Independently compiled, expression-only social vocabulary module.

Provides `digit.socials.select` through the Core service registry.
The consumer submits a NUL-terminated category inside
`digit_socials_request_t` and receives one NUL-terminated UTF-8
expression in `digit_socials_response_t`. Both types are defined in
`socials.h`. The consumer decides intent, frequency and whether to
render the expression. Socials does not interpret conversation, send
messages, infer facts, or access persistence.

Categories: `hearts`, `smiles`, `cats`, `paws`, `playful`,
`comfort`, `sad`, `celebrate`, `gestures`, `text`.
Each category selects pseudo-uniformly from a catalog using kernel
entropy; the same token will not be returned twice consecutively for
the same category within an activation. No external platform API,
automatic posting, or database changes.

Build and test from `modules/socials`:

```sh
make clean && make && make test
sudo make install
```

New modules may be admitted by the Core hotload watcher if configured.
Version starts at 0.1.0. The module cannot add social expressions to
messages on its own: a response-generation consumer must explicitly
invoke its service via Core. Core and IRC are deliberately unchanged.

## Greeting-selection service (v0.2.0)

`digit.socials.greeting` takes `digit_socials_greeting_request_t` and
returns `digit_socials_greeting_response_t` (see `socials.h`).
Allowed trusted relationship classifications are `mommy`, `auntie`,
`sister`, `known`, and `unknown`. The first three use the
operator-assigned forms of address **Mommy** (PoeMei), **Auntie**
(Laurie_Vo), and **Ezra** (Digit's younger sister).
Socials **does not infer those classifications from nicknames**.
Only a trusted identity-aware caller may submit them; this API
does not authenticate callers itself.

Greetings are selected with OS entropy, avoid immediately repeating the
same phrase, and enforce a conservative 60-second cooldown per greeting
class for an activated instance. When on cooldown the service returns
`STNLABZ_MODULE_ERR_INVALID_STATE` with zero output bytes; callers
must not send a message. These are presentation templates, not reasoning
or interpretation. No automatic IRC JOIN handling is installed.
Existing `digit.socials.select` behavior remains unchanged.

The relationship mappings are **designated by the human operator**,
not proven by IRC nickname. Deployment of Socials does not grant anyone
an identity, relationship, or authorization.


## v0.2.1 — authenticated family exchanges

The new `digit.socials.family` service accepts an explicit family kind and an addressed conversational text. It supports the operator-authorized family forms of address Momma, Auntie, and Kitty Sister and selects varied reciprocal greetings, affection, acknowledgments and thanks with expressions drawn from the existing Socials catalogs. It returns NOT_FOUND for non-family classifications or unrecognized social intent, so normal factual conversation is not replaced. There is no JOIN cooldown on interpersonal replies. This is **bounded family-specific language handling**, not general conversational intelligence. The caller is responsible for authenticated relationship classification; Socials does not authenticate accounts. Qualification adds representative positive and negative social-intent cases.
