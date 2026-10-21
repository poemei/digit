#ifndef DIGIT_SQLITE3_GRU_FEED_H
#define DIGIT_SQLITE3_GRU_FEED_H
#include <stdint.h>
#define DIGIT_SQLITE3_GRU_FEED_SERVICE "digit.sqlite3.gru_feed"
#define DIGIT_GRU_FEED_CORPUS 1
#define DIGIT_GRU_FEED_OBSERVATIONS 2
#define DIGIT_GRU_FEED_CONTEXT 3
#define DIGIT_GRU_FEED_PAGE_SIZE 4
typedef struct {
 uint32_t source_type; /* 1 Corpus, 2 Observations, 3 Context */
 int64_t after_id;     /* exclusive, stable row-id cursor */
} digit_gru_feed_query_t;
typedef struct {
 int64_t id;
 char category[64];
 char verification_status[48];
 char source_id[128];
 char text[1025];
} digit_gru_feed_item_t;
typedef struct {
 uint32_t count;
 digit_gru_feed_item_t items[DIGIT_GRU_FEED_PAGE_SIZE];
} digit_gru_feed_result_t;
#endif
