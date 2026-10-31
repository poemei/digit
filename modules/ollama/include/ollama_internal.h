#ifndef DIGIT_OLLAMA_INTERNAL_H
#define DIGIT_OLLAMA_INTERNAL_H

#include <stddef.h>

#define DIGIT_OLLAMA_ENDPOINT "http://127.0.0.1:11434/api/generate"
#define DIGIT_OLLAMA_MODEL "llama3.2:1b"
#define DIGIT_OLLAMA_HTTP_MAX (256U * 1024U)

int digit_ollama_build_request(const char *prompt,
                                char *output, size_t output_size);
int digit_ollama_extract_response(const char *json,
                                   char *output, size_t output_size);
int digit_ollama_requires_research(const char *question);

int digit_ollama_run_selftests(unsigned int *executed,
                                unsigned int *passed,
                                int *negative_executed,
                                int *negative_passed);

#endif
