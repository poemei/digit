#ifndef DIGIT_CONVERSATION_H
#define DIGIT_CONVERSATION_H
#include <stdint.h>
#define DIGIT_CONVERSATION_VERSION 1u
#define DIGIT_CONVERSATION_SERVICE "conversation.generate"
#define DIGIT_CONVERSATION_EVIDENCE_MAX 3u
#define DIGIT_CONVERSATION_HISTORY_MAX 6u
typedef struct {
 char id[65];
 char source[256];
 char text[4096];
} digit_conversation_evidence_t;
typedef struct {
 char sender[128];
 char text[513];
} digit_conversation_turn_t;
typedef struct {
 uint32_t version;
 uint32_t addressed;
 char sender[128];
 char message[513];
 uint32_t evidence_count;
 digit_conversation_evidence_t evidence[DIGIT_CONVERSATION_EVIDENCE_MAX];
 uint32_t history_count;
 digit_conversation_turn_t history[DIGIT_CONVERSATION_HISTORY_MAX];
} digit_conversation_request_t;
typedef struct {
 uint32_t version;
 uint32_t generated; /* 1 = model supplied a response; 0 = model chose no reply */
 char text[512];
} digit_conversation_response_t;
#endif
