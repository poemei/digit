#ifndef DIGIT_DSD_H
#define DIGIT_DSD_H
#include <stdint.h>
#include "module.h"
#define DIGIT_DSD_RECORD_SERVICE "digit.dsd.record"
#define DIGIT_DSD_DIAGNOSE_SERVICE "digit.dsd.diagnose"
#define DIGIT_DSD_MAX_EVENTS 32
/* ABI 1.5 public wire structs. No secrets, raw SQL, or user messages. */
typedef struct {
    uint64_t trace_id,span_id,parent_span_id;
    char module[48],method[64],file[160],status[64],operation[64];
    uint32_t line,elapsed_us,records,selected;
    unsigned char result; /* 0 success, 1 failure, 2 unavailable */
} digit_dsd_event_t;
typedef struct {uint64_t trace_id;digit_dsd_event_t event;} digit_dsd_record_t;
typedef struct {uint64_t trace_id;} digit_dsd_query_t;
typedef struct {
    uint64_t trace_id;
    unsigned int count,failures,dropped;
    int found;
    char summary[512];
    digit_dsd_event_t events[DIGIT_DSD_MAX_EVENTS];
} digit_dsd_result_t;
#endif
