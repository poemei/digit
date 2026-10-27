#ifndef DIGIT_RESEARCH_INTERNAL_H
#define DIGIT_RESEARCH_INTERNAL_H

#include <stddef.h>
#define RESEARCH_INPUT_MAX 512U
#define RESEARCH_OUTPUT_MAX 512U
int research_topic_valid(const char *value);
int research_parse(const char *arguments, char *topic, size_t topic_cap,
                   char *terms, size_t terms_cap);
int research_search(const char *root, const char *topic, const char *terms,
                    char *answer, size_t answer_cap);
int research_selftests(unsigned int *tests, unsigned int *passed,
                       int *negative_executed, int *negative_passed);
#endif
