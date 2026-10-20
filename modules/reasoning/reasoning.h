#ifndef DIGIT_REASONING_H
#define DIGIT_REASONING_H
#include <stdint.h>
#include "../interpretation/interpretation.h"
#include "../sqlite3/corpus_search.h"
#define DIGIT_REASONING_SERVICE "digit.reasoning.evaluate"
#define DIGIT_REASONING_OUTCOME_ABSTAIN 0u
#define DIGIT_REASONING_OUTCOME_EVIDENCE 1u
#define DIGIT_REASONING_OUTCOME_CLARIFY 2u
#define DIGIT_REASONING_OUTCOME_CONFLICT 3u
#define DIGIT_REASONING_OUTCOME_UNKNOWN 4u
#define DIGIT_REASONING_OUTCOME_FAILURE 5u
#define DIGIT_REASONING_OUTCOME_PARTIAL 6u
#define DIGIT_REASONING_MAX_REFERENCES 3u
typedef struct {
 digit_interpretation_result_t interpretation;
 digit_corpus_result_t corpus;
 uint32_t retrieval_attempted;
 uint32_t retrieval_succeeded;
 uint32_t observation_count;
 uint32_t context_count;
} digit_reasoning_request_t;
typedef struct {
 uint32_t outcome;
 uint32_t evidence_count;
 uint32_t uncertainty;
 uint32_t reference_count;
 int64_t corpus_ids[DIGIT_REASONING_MAX_REFERENCES];
 /* Non-linguistic decision signals. A View owns expression. */
 uint32_t requires_verification;
 uint32_t requires_clarification;
} digit_reasoning_result_t;
#endif
