# Recognition 0.1.0 — Initial evidence contract

Independent Digit ABI service: `recognition.evaluate`. Does not own Core,
Router, speaker identity, Corpus storage, Observations, or responses.

The calling module supplies the original utterance, an explicitly retrieved
Corpus record, and up to six same-channel Observation texts. Recognition reports
bounded orthographic overlap and uncertainty separately for Corpus and
conversation. It never interprets overlap as an emotion, intent, truth, consent,
permission to speak, or semantic equivalence.

This initial capability deliberately does not generate text or infer affect.
It is a measurable foundation, **not** human conversational understanding.
It is not wired into Ask; independently test and accept the recognition contract
before authorizing behavioral integration. No new learning interface or source
data is introduced.

Build: `make -C modules/recognition clean all test`.
