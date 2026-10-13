#ifndef DIGIT_GFM_H
#define DIGIT_GFM_H
#include <stddef.h>
#include "stnlabz_abi/module.h"
#define GFM_CONTRACT_VERSION 1U
#define GFM_DOCUMENT_MAX 8192U
typedef struct {
 unsigned int contract_version;
 char document[GFM_DOCUMENT_MAX];
} digit_gfm_request_t;
typedef struct {
 unsigned int contract_version;
 unsigned headings,lists,tables,code_fences,links;
 unsigned prose_lines,code_lines;
 int balanced_fences;
 char summary[512];
} digit_gfm_response_t;
stnlabz_module_result_t digit_gfm_analyze(const void *,size_t,void *,size_t,size_t *,void *);
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
#endif
