# Digit Interpretation — initial lexical capability

Native-C ABI 1.5 independent module. Registers `digit.interpretation`.

This initial capability deterministically extracts bounded lexical terms,
question/statement surface classification, and indicators for negation,
time words, references, contrast, and causal vocabulary. It explicitly
does **not** implement semantic comprehension, pronoun resolution,
conversation participation, reasoning, or natural-language generation.

It does not invoke Ollama, an LLM, neural inference, an external service,
a database, or a hardcoded conversational response table.

The module reports syntactic indicators, not the truth or meaning of an
assertion. References are flagged as unresolved, not guessed.

Build: `make`. `make test` currently verifies compilation only.
The module descriptor includes 24 offline lexical checks, but real
qualification and operator acceptance must be verified separately.

The current Conversation module is not modified or connected to this
service. Do not activate or deploy until integration requirements,
qualification evidence, and human operator acceptance are established.
