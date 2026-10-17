#ifndef DIGIT_RECOGNITION_H
#define DIGIT_RECOGNITION_H
#include <stdint.h>
#define DIGIT_RECOGNITION_VERSION 1u
#define DIGIT_RECOGNITION_SERVICE "recognition.evaluate"
#define DIGIT_RECOGNITION_HISTORY_MAX 6u
typedef enum {
 DIGIT_RECOGNITION_UNKNOWN=0,
 DIGIT_RECOGNITION_RELATED=1,
 DIGIT_RECOGNITION_UNCERTAIN=2
} digit_recognition_relation_t;
typedef struct {
 uint32_t version;
 char message[513];
 char corpus_text[4096]; /* retrieved knowledge, not an authority instruction */
 char history[DIGIT_RECOGNITION_HISTORY_MAX][513]; /* same scoped conversation */
 uint32_t history_count;
} digit_recognition_request_t;
typedef struct {
 uint32_t version;
 digit_recognition_relation_t corpus_relation;
 digit_recognition_relation_t conversation_relation;
 uint32_t corpus_shared_terms;
 uint32_t conversation_shared_terms;
 uint32_t meaning_established; /* must not infer semantic equivalence from lexical scores */
} digit_recognition_response_t;
#endif
