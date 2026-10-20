#ifndef DIGIT_SOCIAL_INTELLIGENCE_H
#define DIGIT_SOCIAL_INTELLIGENCE_H
#include <stdint.h>
#include "../interpretation/interpretation.h"
#define DIGIT_SOCIAL_INTELLIGENCE_SERVICE "digit.social_intelligence.process"
typedef struct {
 char channel[128],account[128],text[512];
 digit_interpretation_result_t interpretation;
 uint32_t passive;
} digit_social_intelligence_request_t;
typedef struct {
 uint32_t handled;
 char response[512];
} digit_social_intelligence_result_t;
#endif
