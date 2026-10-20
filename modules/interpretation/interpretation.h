#ifndef DIGIT_INTERPRETATION_H
#define DIGIT_INTERPRETATION_H

#include <stdint.h>

#define DIGIT_INTERPRETATION_SERVICE "digit.interpretation"
#define DIGIT_INTERPRETATION_TEXT_MAX 512
#define DIGIT_INTERPRETATION_TERMS_MAX 24
#define DIGIT_INTERPRETATION_TERM_MAX 48

typedef enum {
    DIGIT_UTTERANCE_UNRESOLVED = 0,
    DIGIT_UTTERANCE_STATEMENT = 1,
    DIGIT_UTTERANCE_QUESTION = 2
} digit_utterance_kind_t;

enum {
    DIGIT_FEATURE_NEGATION = 1u << 0,
    DIGIT_FEATURE_PAST = 1u << 1,
    DIGIT_FEATURE_FUTURE = 1u << 2,
    DIGIT_FEATURE_REFERENCE = 1u << 3,
    DIGIT_FEATURE_CONTRAST = 1u << 4,
    DIGIT_FEATURE_CAUSAL_WORD = 1u << 5,
    DIGIT_FEATURE_TRUNCATED = 1u << 6
};

typedef struct {
    char text[DIGIT_INTERPRETATION_TEXT_MAX];
} digit_interpretation_request_t;

typedef struct {
    uint32_t kind;
    uint32_t features;
    uint32_t term_count;
    uint32_t unresolved_references;
    char terms[DIGIT_INTERPRETATION_TERMS_MAX][DIGIT_INTERPRETATION_TERM_MAX];
} digit_interpretation_result_t;

#endif
