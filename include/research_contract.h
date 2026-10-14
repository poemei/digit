#ifndef DIGIT_RESEARCH_CONTRACT_H
#define DIGIT_RESEARCH_CONTRACT_H
#include <stdint.h>
#define DIGIT_RESEARCH_SERVICE "research.retrieve"
#define DIGIT_RESEARCH_CONTRACT_VERSION 1u
#define DIGIT_RESEARCH_URL_MAX 1024
#define DIGIT_RESEARCH_MISSION_MAX 128
#define DIGIT_RESEARCH_EXCERPT_MAX 8192
typedef struct {
 uint32_t version;
 char mission[DIGIT_RESEARCH_MISSION_MAX];
 char url[DIGIT_RESEARCH_URL_MAX];
} digit_research_request_t;
typedef struct {
 uint32_t version;
 int http_status;
 int source_truncated;
 char retrieved_at[32];
 char content_type[96];
 char url[DIGIT_RESEARCH_URL_MAX];
 char excerpt[DIGIT_RESEARCH_EXCERPT_MAX];
} digit_research_result_t;
#endif
