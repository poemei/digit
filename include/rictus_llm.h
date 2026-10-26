#ifndef RICTUS_LLM_H
#define RICTUS_LLM_H

#define RICTUS_LLM_GENERATE_SERVICE "rictus.llm.generate"

#define RICTUS_LLM_PROMPT_MAX 2048
#define RICTUS_LLM_RESPONSE_MAX 512

typedef struct {
    char prompt[RICTUS_LLM_PROMPT_MAX];
} rictus_llm_request_t;

typedef struct {
    char text[RICTUS_LLM_RESPONSE_MAX];
} rictus_llm_response_t;

#endif
