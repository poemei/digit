#ifndef DIGIT_CONTEXT_H
#define DIGIT_CONTEXT_H
#include <stdint.h>
#define DIGIT_CONTEXT_STORE_SERVICE "digit.sqlite3.context_store"
#define DIGIT_CONTEXT_RECENT_SERVICE "digit.sqlite3.context_recent"
#define DIGIT_CONTEXT_MAX 8
#define DIGIT_CONTEXT_TEXT_MAX 512
/* The context table's existing subject_type/subject_id bind to the
 * conversations table. Source ID is an observations.event_id. */
typedef struct {
    char channel[128];
    char source_account[128];
    char sender[128];
    char text[DIGIT_CONTEXT_TEXT_MAX];
    uint32_t kind; /* 1=statement, 2=question; observational, not belief */
} digit_context_store_request_t;
typedef struct { char channel[128]; } digit_context_recent_query_t;
typedef struct {
    char source_id[96];
    char context_type[32];
    char value[DIGIT_CONTEXT_TEXT_MAX];
} digit_context_item_t;
typedef struct {
    uint32_t count;
    digit_context_item_t items[DIGIT_CONTEXT_MAX];
} digit_context_recent_result_t;
#endif
