#ifndef DIGIT_ASK_CONTEXT_H
#define DIGIT_ASK_CONTEXT_H
#include <stdint.h>
#define DIGIT_ASK_CONTEXT_MAGIC 0x41534b31u
typedef struct {
 uint32_t magic;
 char question[513];
 char previous_request[64];
 char previous_route[64];
 char previous_service[128];
 int previous_outcome;
 int previous_service_result;
 int has_previous;
 char transport[16];
 char channel[128];
 char sender[128];
} digit_ask_context_t;
typedef struct {
 char request_id[64];
 char route[64];
 char service[128];
 int outcome;
 int service_result;
} digit_scoped_diagnostic_t;
#endif
