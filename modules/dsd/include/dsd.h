#ifndef DIGIT_DSD_H
#define DIGIT_DSD_H
#include "stnlabz_abi/module.h"
#define DIGIT_DSD_TRACE_MAX 8
#define DIGIT_DSD_TEXT_MAX 128
typedef struct {
 char component[32];
 char decision[32];
 char file[96];
 char function[64];
 unsigned int line;
 char reason[DIGIT_DSD_TEXT_MAX];
} digit_dsd_observation_t;
typedef struct {
 char request_id[64];
 unsigned int count;
 digit_dsd_observation_t observations[DIGIT_DSD_TRACE_MAX];
} digit_dsd_trace_t;
typedef struct {
 char request_id[64];
 unsigned int observations;
 int has_failure;
 char report[2048];
} digit_dsd_report_t;
/* Observations supplied by their actual decision points; DSD only reports them. */
stnlabz_module_result_t digit_dsd_report(const void *request,size_t request_size,
 void *response,size_t response_size,size_t *response_used,void *context);
/* Core-mediated, read-only surveillance tick: output contains an alert only
   when a new compiler-confirmed defect is found. */
#define DSD_WATCH_TEXT_MAX 2048
typedef struct { unsigned int contract_version; char text[DSD_WATCH_TEXT_MAX]; } digit_dsd_watch_response_t;
stnlabz_module_descriptor_t digit_dsd_descriptor(void);
#endif
