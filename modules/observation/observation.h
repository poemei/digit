#ifndef DIGIT_OBSERVATION_H
#define DIGIT_OBSERVATION_H
#define DIGIT_OBSERVATION_RECORD_SERVICE "digit.observation.record"
#define DIGIT_OBSERVATION_INSERT_SERVICE "digit.sqlite3.observation_insert"
#define DIGIT_OBSERVATION_RECENT_SERVICE "digit.sqlite3.observation_recent"
#define DIGIT_OBSERVATION_RECENT_MAX 6
typedef struct {char transport[32],channel[128],sender[128],account[128],text[512];} digit_observation_record_t;
typedef struct {char channel[128];} digit_observation_recent_query_t;
typedef struct {char sender[128],text[512];} digit_observation_recent_item_t;
typedef struct {unsigned count;digit_observation_recent_item_t items[DIGIT_OBSERVATION_RECENT_MAX];} digit_observation_recent_result_t;
#endif
