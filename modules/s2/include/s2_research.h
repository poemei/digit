#ifndef DIGIT_S2_RESEARCH_H
#define DIGIT_S2_RESEARCH_H
#include <stddef.h>
/* Source-backed, bounded human-language provenance summary; never includes the access key. */
int s2_httpbl_review(const char *json,size_t length,char *out,size_t cap);
#endif
