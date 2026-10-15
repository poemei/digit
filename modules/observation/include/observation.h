#ifndef DIGIT_OBSERVATION_H
#define DIGIT_OBSERVATION_H
#include <stdint.h>
#define DIGIT_OBSERVATION_SERVICE "observation.record"
#define DIGIT_OBSERVATION_VERSION 1U
#define DIGIT_OBSERVATION_READ_SERVICE "observation.recent"
#define DIGIT_OBSERVATION_READ_VERSION 1U
#define DIGIT_OBSERVATION_RECENT_MAX 6U
typedef struct {
 uint32_t version;
 char transport[16];
 char channel[128];
 char sender[128];
 char event_id[160];
 char text[1024];
} digit_observation_t;
typedef struct {
 uint32_t version;
 char transport[16];
 char channel[128];
 uint32_t limit;
} digit_observation_recent_request_t;
typedef struct {
 char time[32];
 char sender[128];
 char event_id[160];
 char text[1024];
} digit_observation_recent_item_t;
typedef struct {
 uint32_t version;
 uint32_t count;
 digit_observation_recent_item_t items[DIGIT_OBSERVATION_RECENT_MAX];
} digit_observation_recent_response_t;
#endif
