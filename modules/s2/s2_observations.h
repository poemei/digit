#ifndef DIGIT_S2_OBSERVATIONS_H
#define DIGIT_S2_OBSERVATIONS_H
#include <stdint.h>
#define DIGIT_S2_OBSERVATIONS_SERVICE "digit.sqlite3.s2_observations"
#define DIGIT_S2_OBSERVATIONS_TEXT_MAX 512
typedef struct { int64_t after_id; } digit_s2_observations_query_t;
typedef struct {
 int64_t id;
 char text[DIGIT_S2_OBSERVATIONS_TEXT_MAX];
} digit_s2_observations_result_t;
#endif
