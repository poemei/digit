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
/* Versioned extension. The legacy feed and its ABI remain unchanged. 
 * One UTF-8-safe fragment of a single SQLite record per request.
 * after_id identifies the last fully delivered record; offset is the
 * next byte of the new record. The caller must preserve source identity
 * and only checkpoint after more==0. */
#define DIGIT_SQLITE3_GRU_FEED_CHUNK_SERVICE "digit.sqlite3.gru_feed_chunk"
#define DIGIT_GRU_FEED_CHUNK_BYTES 1024
typedef struct {
 uint32_t source_type;
 int64_t after_id;
 uint32_t offset;
} digit_gru_feed_chunk_query_t;
typedef struct {
 int64_t id;
 uint32_t found;
 uint32_t offset, length, total_bytes, next_offset, more;
 uint64_t fingerprint;
 char category[64], verification_status[48], source_id[128];
 char text[DIGIT_GRU_FEED_CHUNK_BYTES+1];
} digit_gru_feed_chunk_result_t;
#endif
