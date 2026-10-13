#ifndef DIGIT_MODEL_EXPRESSION_H
#define DIGIT_MODEL_EXPRESSION_H
#include "stnlabz_abi/module.h"
#define DIGIT_EXPRESSION_TEXT_MAX 64
typedef enum { DIGIT_EXPRESSION_KNOWN=0, DIGIT_EXPRESSION_UNKNOWN, DIGIT_EXPRESSION_INVALID } digit_expression_status_t;
typedef struct { char expression[DIGIT_EXPRESSION_TEXT_MAX]; } digit_expression_request_t;
typedef struct {
 digit_expression_status_t status;
 char expression[DIGIT_EXPRESSION_TEXT_MAX];
 char meaning[128];
 char evidence[64];
 char source_file[64];
 char source_function[64];
 unsigned int source_line;
} digit_expression_response_t;
stnlabz_module_descriptor_t digit_expression_descriptor(void);
stnlabz_module_result_t digit_expression_qualify(stnlabz_module_qualification_result_t *result);
stnlabz_module_result_t digit_expression_start(const stnlabz_module_host_t *host);
stnlabz_module_result_t digit_expression_stop(void);
stnlabz_module_result_t digit_expression_lookup(const void *request,size_t request_size,
 void *response,size_t response_size,size_t *response_used,void *context);
#endif
