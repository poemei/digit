#ifndef DIGIT_GRU_LEARNING_H
#define DIGIT_GRU_LEARNING_H
#include <stdint.h>
#define DIGIT_GRU_LEARNING_PREVIEW_SERVICE "digit.gru.learning_preview"
/* [AI:GPT-6 | 2026-10-21 05:41:12 UTC] */
#define DIGIT_GRU_LEARNING_SUBMIT_SERVICE "digit.gru.learning_submit"
typedef struct {
 char channel[128], event_id[96], response[1025];
 uint32_t approved;
} digit_gru_learning_submit_request_t;
/* [End AI:GPT-6] */
#define DIGIT_GRU_LEARNING_OBSERVATIONS_MAX 6
#define DIGIT_GRU_LEARNING_CORPUS_MAX 3
typedef struct {
 char channel[128]; /* optional; empty skips observations */
 char query[512];   /* optional; empty skips Corpus search */
} digit_gru_learning_preview_request_t;
typedef struct {
 char sender[128], text[512];
} digit_gru_learning_observation_t;
typedef struct {
 int64_t id;
 char category[48], verification_status[48], excerpt[360];
} digit_gru_learning_corpus_t;
typedef struct {
 uint32_t observations_count, corpus_count;
 uint32_t observations_available, corpus_available;
 uint32_t approved, submitted, trained; /* always zero in preview */
 digit_gru_learning_observation_t observations[6];
 digit_gru_learning_corpus_t corpus[3];
} digit_gru_learning_preview_result_t;
#endif
