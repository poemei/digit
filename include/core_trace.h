#ifndef DIGIT_CORE_TRACE_H
#define DIGIT_CORE_TRACE_H
#include <stdint.h>
/* Thread-local correlation, no capped registry or global mutable record collection. */
uint64_t digit_trace_begin(void);
void digit_trace_end(uint64_t previous);
uint64_t digit_trace_current(void);
int digit_trace_record_at(const char *component, const char *operation,
    const char *decision, const char *result, const char *reason,
    const char *file, const char *function, unsigned int line);
#define DIGIT_TRACE(component, operation, decision, result, reason) \
    digit_trace_record_at(component, operation, decision, result, reason, \
                          __FILE__, __func__, (unsigned int)__LINE__)
#endif
