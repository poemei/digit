# Reasoning 0.1.0 — candidate

Independent read-only evidence relationship evaluator using the accepted Reasoning Evidence Contract. No language model, external retrieval, hardcoded intent lexicon or generated responses.

The `reasoning.evaluate` service accepts caller-supplied message, transport/channel, at most six same-scope observation records, and one optional Corpus record with retrieval states. It returns the evidence record IDs, Corpus provenance and candidate token-intersection relevance. Token intersection is only a **candidate association**; it is neither semantics, intent, verified truth, criticism detection, nor permission to respond. In particular, overlapping common words can be false positives and paraphrases without shared tokens can be missed. `NONE` means no surface overlap in supplied evidence, not proof of irrelevance.

No direct Corpus/Observation calls occur in this service; the caller must establish source and channel scope before submission. No Ask/IRC/Slack integration, automatic learning, writes, or runtime authority changes are included. A production integration must separately verify evidence scoping and relevance semantics before using the result to decide speech/silence. Module qualification currently exercises only baseline descriptor gates; the test executable contains the functional negative and positive cases.

Build: `make -C modules/reasoning clean all test`. Candidate installation requires operator-controlled evaluation and permission.
