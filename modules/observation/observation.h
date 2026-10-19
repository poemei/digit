#ifndef DIGIT_OBSERVATION_H
#define DIGIT_OBSERVATION_H
#define DIGIT_OBSERVATION_RECORD_SERVICE "digit.observation.record"
#define DIGIT_OBSERVATION_INSERT_SERVICE "digit.sqlite3.observation_insert"
#define DIGIT_OBSERVATION_FIELD 512
typedef struct {
 char transport[16];
 char channel[128];
 char sender[128];
 char account[128];
 char text[DIGIT_OBSERVATION_FIELD];
} digit_observation_record_t;
/* Read-only recent IRC context returned in chronological order. */
#define DIGIT_OBSERVATION_RECENT_SERVICE "digit.sqlite3.observation_recent"
#define DIGIT_OBSERVATION_RECENT_MAX 6
typedef struct { char channel[128]; } digit_observation_recent_query_t;
typedef struct {
 char sender[128];
 char text[DIGIT_OBSERVATION_FIELD];
} digit_observation_recent_item_t;
typedef struct {
 unsigned count;
 digit_observation_recent_item_t items[DIGIT_OBSERVATION_RECENT_MAX];
} digit_observation_recent_result_t;
#endif
