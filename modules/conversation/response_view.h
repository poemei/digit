#ifndef DIGIT_CONVERSATION_RESPONSE_VIEW_H
#define DIGIT_CONVERSATION_RESPONSE_VIEW_H
#include <stddef.h>
#include "../reasoning/reasoning.h"
int digit_response_render(const digit_reasoning_result_t *decision,
 const digit_corpus_result_t *evidence,const digit_interpretation_result_t *question,
 const char *original_text,char *output,size_t capacity,size_t *used);
#endif
