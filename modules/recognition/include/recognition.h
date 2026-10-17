#ifndef DIGIT_RECOGNITION_H
#define DIGIT_RECOGNITION_H
#include <stdint.h>
#define DIGIT_RECOGNITION_VERSION 2u
#define DIGIT_RECOGNITION_SERVICE "recognition.evaluate"
#define DIGIT_RECOGNITION_HISTORY_MAX 6u
typedef enum {
 DIGIT_RECOGNITION_UNKNOWN=0,
 DIGIT_RECOGNITION_RELATED=1,
 DIGIT_RECOGNITION_UNCERTAIN=2
} digit_recognition_relation_t;
typedef struct {
 char sender[128];
 char text[513];
} digit_recognition_turn_t;
typedef struct {
 uint32_t version;
 char sender[128]; /* current speaker, supplied by the transport */
 uint32_t addressed; /* Interpretation's direct-address assessment */
 char message[513];
 char corpus_text[4096]; /* retrieved knowledge, not an authority instruction */
 digit_recognition_turn_t history[DIGIT_RECOGNITION_HISTORY_MAX]; /* scoped, attributed turns */
 uint32_t history_count;
} digit_recognition_request_t;
typedef struct {
 uint32_t version;
 digit_recognition_relation_t corpus_relation;
 digit_recognition_relation_t conversation_relation;
 uint32_t corpus_shared_terms;
 uint32_t conversation_shared_terms;
 uint32_t meaning_established; /* similarity is not semantic meaning */
 uint32_t speaker_continuity; /* current speaker has a prior turn */
 uint32_t subject_continuity; /* topic evidence across distinct speakers */
 uint32_t direct_address; /* caller-supplied classification, not authorization */
} digit_recognition_response_t;
#endif
