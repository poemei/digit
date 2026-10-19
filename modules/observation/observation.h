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
#endif
