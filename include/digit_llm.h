#ifndef DIGIT_LLM_H
#define DIGIT_LLM_H

#define DIGIT_LLM_GENERATE_SERVICE "digit.llm.generate"

#define DIGIT_LLM_PROMPT_MAX 2048
#define DIGIT_LLM_RESPONSE_MAX 512

typedef struct {
    char prompt[DIGIT_LLM_PROMPT_MAX];
} digit_llm_request_t;

typedef struct {
    char text[DIGIT_LLM_RESPONSE_MAX];
} digit_llm_response_t;

#endif
