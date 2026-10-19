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
