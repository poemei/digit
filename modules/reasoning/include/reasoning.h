#ifndef DIGIT_REASONING_H
#define DIGIT_REASONING_H
#include <stdint.h>
#define DIGIT_REASONING_VERSION 1u
#define DIGIT_REASONING_SERVICE "reasoning.evaluate"
#define DIGIT_REASONING_ITEMS 6u
typedef enum {DIGIT_REASONING_UNAVAILABLE=0,DIGIT_REASONING_EMPTY=1,DIGIT_REASONING_PRESENT=2} digit_reasoning_evidence_state_t;
typedef enum {DIGIT_REASONING_UNKNOWN=0,DIGIT_REASONING_CORPUS_SUPPORTED=1,DIGIT_REASONING_EVIDENCE_UNAVAILABLE=2} digit_reasoning_knowledge_t;
typedef enum {DIGIT_REASONING_UNDETERMINED=0,DIGIT_REASONING_CONTEXTUAL=1,DIGIT_REASONING_NONE=2} digit_reasoning_relevance_t;
typedef struct {
 char event_id[160];
 char sender[128];
 char text[1024];
} digit_reasoning_observation_t;
typedef struct {
 uint32_t version;
 char message[513];
 char transport[16];
 char channel[128];
 char sender[128];
 digit_reasoning_evidence_state_t observation_state;
 uint32_t observation_count;
 digit_reasoning_observation_t observations[DIGIT_REASONING_ITEMS];
 digit_reasoning_evidence_state_t corpus_state;
 char corpus_id[65];
 char corpus_source[256];
 char corpus_text[4096];
} digit_reasoning_request_t;
typedef struct {
 uint32_t version;
 digit_reasoning_knowledge_t knowledge;
 digit_reasoning_relevance_t relevance;
 uint32_t matched_observations;
 char observation_ids[DIGIT_REASONING_ITEMS][160];
 char corpus_id[65];
 char corpus_source[256];
 /* No generated response text or authority grant. */
} digit_reasoning_response_t;
#endif
