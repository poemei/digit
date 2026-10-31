#ifndef DIGIT_RESEARCH_H
#define DIGIT_RESEARCH_H
#include "stnlabz_abi/module.h"
#define RESEARCH_QUERY_MAX 256
#define RESEARCH_EVIDENCE_MAX 8192
#define RESEARCH_DENIED 1
#define RESEARCH_UNAVAILABLE 2
#define RESEARCH_EVIDENCE 3
typedef struct {char query[RESEARCH_QUERY_MAX];} research_request_t;
typedef struct {
 int status;
 unsigned sources;
 char retrieved_utc[40];
 char evidence[RESEARCH_EVIDENCE_MAX];
} research_response_t;
#endif
