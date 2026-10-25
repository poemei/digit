#ifndef DIGIT_ST1_H
#define DIGIT_ST1_H
/* [AI:GPT-6 | 2026-10-25 18:20:00 UTC] */
#include "module.h"
#define DIGIT_ST1_SERVICE "st1.query"
#define DIGIT_ST1_PATH "/opt/digit/research/st1/research.tsv"
#define DIGIT_ST1_RESULTS 8
typedef struct {
 char id[64],kind[16],title[160],url[256],date[11],summary[512];
 char evidence[16],relevance[8],systems[128],impact[16],milestone[192];
 unsigned int revision;
 char recorded[21];
} digit_st1_record_t;
typedef struct {char action[16],term[128];} digit_st1_query_t;
typedef struct {unsigned int count;int error;digit_st1_record_t records[DIGIT_ST1_RESULTS];} digit_st1_query_result_t;
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
/* [End AI:GPT-6] */
#endif
