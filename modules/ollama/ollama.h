#ifndef DIGIT_OLLAMA_H
#define DIGIT_OLLAMA_H
/* [AI:GPT-6 | 2026-10-26 02:00:00 UTC] */
#include <stdint.h>
#include "module.h"
#define DIGIT_OLLAMA_GENERATE_SERVICE "digit.ollama.generate"
#define DIGIT_OLLAMA_MODEL "llama3.2:1b"
#define DIGIT_OLLAMA_PROMPT_MAX 512
#define DIGIT_OLLAMA_EVIDENCE_MAX 512
#define DIGIT_OLLAMA_OUTPUT_MAX 1024
typedef struct {
 char question[DIGIT_OLLAMA_PROMPT_MAX];
 char evidence[DIGIT_OLLAMA_EVIDENCE_MAX];
 uint32_t max_tokens; /* 0 = 128; upper bound 256 */
 uint32_t evidence_required; /* 1 for factual; 0 for nonfactual language */
} digit_ollama_request_t;
typedef struct {
 char text[DIGIT_OLLAMA_OUTPUT_MAX];
 uint32_t generated_bytes;
 uint32_t unverified; /* always 1: caller must validate output */
} digit_ollama_result_t;
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
/* [End AI:GPT-6] */
#endif
