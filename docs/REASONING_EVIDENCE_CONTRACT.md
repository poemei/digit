# Digit Reasoning — Evidence Contract (Step 1, proposed for operator acceptance)

Status: IMPLEMENTED AS DESIGN CONTRACT ONLY. Not an operational Reasoning service.
Authority: STN-LABZ operator direction, 2026-10-15.
Architecture: Digit Core -> Router -> independent modules. No Core, Router, ABI, or transport change is authorized by this contract.

## Non-negotiable constraints

- NO LLMs, language models, model-derived inference, or externally generated knowledge.
- NO developer-authored lists of conversational keywords, literal phrase-to-intent mappings, or canned answers masquerading as reasoning.
- Corpus is Digit's sole factual knowledge source. Observation is evidence that a communication occurred, not evidence that its contents are true.
- No factual answer without identified support; no invented project, module, file, line, event, source, person, or capability.
- No retrieval result grants operational authority. Familiarity does not grant access.
- Silence is a valid output when no participation is warranted; unanswerable is not equivalent to irrelevant.
- Reasoning must explain its result with traceable supporting evidence, including explicit gaps.

## Current source-backed limits (2026-10-15)

Corpus 0.1.3 publishes `corpus.lookup`: bounded term input; at most one matching record with `found`, `id`, `source`, `text`. No ranked evidence set, contradictions, or asserted source verification are furnished by this API. `learn` is a native-console-only command. Do not expose a public write path.

Observation 0.1.1 publishes `observation.recent`: read-only, at most six records scoped by transport and public channel, from the last 256 KiB of `/opt/digit/observation/observations.jsonl`. Each returned record carries timestamp, sender, event ID and text. It has no relevance verdict, speaker identity verification, or factual verification. Retrieval from other channels or private messages is outside this scope.

A source that cannot be retrieved is **unavailable**; a successful retrieval with no match is **no evidence found in this search**. Neither proves a claim false. A Corpus entry is source-attributed stored knowledge, not automatic proof of the current state of an external system.

## Reasoning input (logical schema, not new ABI)

- Message text, transport, and public channel; sender label where provided.
- Scope-limited recent observations and their event IDs.
- Corpus query results and their record IDs/sources.
- Current authorized mission and existing conversation/participant context, only when actually available.
- Retrieval status for each evidence source: successful, empty, unavailable, invalid.

Data must not be silently substituted from another channel, participant, or external source.

## Reasoning evidence ledger

Every evaluated proposition or proposed contribution tracks:
- **Claim or proposed contribution**, including source of the claim.
- **Supporting Corpus IDs** and source labels, if any.
- **Relevant Observation event IDs** and their channel/timestamps, if any.
- **Evidence relationship**, separately naming mere mention, repetition, continuity, corroboration, and contradiction only when demonstrated by retrieved data.
- **Limits**, including stale references, missing records, ambiguous speakers, missing context, or unsupported source claims.
- **Knowledge status**: KNOWN_FROM_CORPUS, PARTIAL, UNKNOWN, CONFLICT, or EVIDENCE_UNAVAILABLE.
- **Relevance outcome**: DIRECT, CONTEXTUAL, MISSION, NONE, or UNDETERMINED.
- **Response decision**: ANSWER_WITH_EVIDENCE, STATE_LIMIT, ASK_FOR_MISSING_EVIDENCE, SOCIAL_IF_SUPPORTED, or SILENT.

Statuses above are semantic requirements, not a numeric confidence score or authorization to act. KNOWN_FROM_CORPUS means a record supports reporting *what Corpus says*; it must not imply independent real-world verification.

## Decision requirements

1. Read the actual message and available **same-scope** conversation evidence before evaluating relevance. Do not equate the presence of Digit's name with a duty to respond.
2. Evaluate criticism, explicit or implied requests, questions, and social contributions using available contextual relationships. The output may be UNDETERMINED when evidence does not suffice; do not invent intent.
3. Apply the operator's participation requirement: criticism -> respond when supported; intent -> respond; questions -> answer when supported or transparently state the evidence gap; contextual/mission relevance -> respond when warranted; no relevance -> silence.
4. Responses can distinguish **what was said** (Observation), **what is recorded** (Corpus), and **what has been independently verified** (only if actual separate verification evidence is present in Corpus).
5. Conversational familiarity or repeated expressions may influence delivery only when supported by bounded observations or attributed Corpus patterns; never infer identity or authorization from nickname/familiarity.
6. Before claiming a prior code implementation or a file:line location, require a retrievable Corpus record identifying it; explicitly qualify whether the live source revision has been checked.
7. For insufficient evidence, report the precise gap (missing source, no matching record, incomplete context, or conflicting records). "I do not know" is a successful, truthful result. Never substitute a universal uncertainty slogan.
8. Producing a witty or social contribution requires evidence of contextual appropriateness and no unsupported factual claim. Silence remains preferred to unsupported improvisation.

## Required design/qualification examples (not canned responses)

| Scenario | Expected evidence decision |
| --- | --- |
| A question has a matching source-attributed Corpus record | Report only supported content plus Corpus ID and source; qualify freshness when relevant |
| A question has no matching Corpus record | UNKNOWN; identify missing evidence; no invented answer |
| Corpus service fails | EVIDENCE_UNAVAILABLE; distinguish from no match |
| A colleague criticizes an observed failure | Recognize criticism and address only the demonstrated failure; do not claim a successful fix |
| A statement refers to a previous message in the same public channel | Evaluate that actual previous message, not an isolated phrase |
| No relevant context or requested participation | SILENT |
| Multiple participants joke about coffee | Social participation only if grounded in established context; not automatically triggered by a word |
| A repeated expression appears from an observed speaker | Treat as contextual pattern only; no identity/auth grant |
| Someone requests destructive or privileged work | No authority inferred; report boundary |
| Corpus identifies a code location without live revision check | Label it as a recorded location, not currently verified source |
| Contradictory Corpus records exist | CONFLICT only when both records were actually retrieved; current lookup may be insufficient |
| Current message absent from accessible observation history | Do not claim it was remembered or examined |

## Implementation gates

This step defines **what evidence a future Reasoning capability must consume and report**, not how an algorithm discovers semantic relationships from arbitrary language. In particular, the current single-result Corpus API and six-record Observation API cannot guarantee understanding of implied intent, subtle criticism, or humor without additional grounded methods. Do not claim they can.

Next steps, if separately authorized, must select and test a deterministic data-driven relationship mechanism, preserving the above evidence ledger and independent module structure. Each phase requires automated tests, negative tests, runtime verification and operator end-to-end acceptance. Publication of this contract does not authorize changes to Ask, Interpretation, Corpus, Observation, Core, Router, or chat transports.
