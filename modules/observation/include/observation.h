#ifndef DIGIT_OBSERVATION_H
#define DIGIT_OBSERVATION_H
#include <stdint.h>
#define DIGIT_OBSERVATION_SERVICE "observation.record"
#define DIGIT_OBSERVATION_VERSION 1U
typedef struct {
 uint32_t version;
 char transport[16];
 char channel[128];
 char sender[128];
 char event_id[160];
 char text[1024];
} digit_observation_t;
#endif
