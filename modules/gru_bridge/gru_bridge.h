#ifndef DIGIT_GRU_BRIDGE_H
#define DIGIT_GRU_BRIDGE_H
#include <stdint.h>
#define DIGIT_GRU_GENERATE_SERVICE "digit.gru.generate"
#define DIGIT_GRU_LEARN_SERVICE "digit.gru.learn"
#define DIGIT_GRU_PROMPT_MAX 1025
#define DIGIT_GRU_TEXT_MAX 1025
typedef struct {
 char prompt[DIGIT_GRU_PROMPT_MAX]; /* UTF-8, 1..1024 bytes */
 uint32_t length;                 /* 1..256; 0 means API default */
 uint64_t seed;                   /* 0 means API default */
 double temperature;              /* 0 means API default */
 double penalty;                  /* 0 means API default */
} digit_gru_generate_request_t;
typedef struct {
 char text[DIGIT_GRU_TEXT_MAX];
 uint32_t generated_bytes;
 uint32_t experimental;
} digit_gru_generate_result_t;
typedef struct {
 char input[1025],response[1025],source[128];
 uint32_t approved; /* explicit operator approval required */
} digit_gru_learn_request_t;
typedef struct {
 uint32_t accepted,trained;
 char status[64];
} digit_gru_learn_result_t;
#endif
