Ask 0.1.3: /ask remains the sole public conversation route. An in-memory 128-bit server-issued token scopes the previous request per chat. Ask delegates 'diagnose my last request' to DSD dsd.scoped. No shared log search or administrative privilege. Unsupported intent remains unsupported. Build: make -C modules/ask clean all test. Verify independent sessions and denied invalid tokens. Operator acceptance pending.

## Corpus-backed factual answers (0.1.4)

Ask recognizes `what is <term>?`, strips trailing question punctuation, and invokes `corpus.lookup` via the existing host ABI. Corpus owns deterministic retrieval from `/opt/digit/corpus/corpus.tsv`; Ask returns the exact matched record text with record ID and source attribution. No hardcoded STN-LABZ definition, TSV modifications, Core, Router, or IRC changes. Missing records and unavailable Corpus explicitly report their limitations. Answer length is bounded by the transport response capacity; overlong entries are not silently truncated into invented summaries.

## Core-owned agent version queries (0.1.6)

Ask recognizes `version`, `version?`, `digit version`, `what version are you?`, and `what is your version?`. All request `core.version.read` and return the actual running Core's version in the form `Digit 1.0.0`, without making Ask or IRC the version authority. Core binary and Ask need installing for the feature to be visible.

## Interpretation integration (0.1.5)

When Ask's existing exact operations do not match, it invokes the independent `interpretation.classify` ABI service. A capabilities intent produces an accurate bounded summary of Ask's public operations; a knowledge intent extracts a subject and performs read-only `corpus.lookup`. Unknown intent explicitly requests clarification. Neither the classifier nor Ask grants privileges or exposes Corpus writes. The initial Interpretation 0.1.0 classifier is lexical and bounded, **not** general language-model reasoning; validate novel phrasings and negative inputs before acceptance. If Interpretation is inactive, Ask reports it unavailable.
