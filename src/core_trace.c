#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "core_trace.h"
#include "logging.h"

static atomic_uint_fast64_t next_trace = ATOMIC_VAR_INIT(1);
static _Thread_local uint64_t current_trace;
uint64_t digit_trace_current(void) { return current_trace; }
uint64_t digit_trace_begin(void)
{
    uint64_t previous = current_trace;
    if (!previous) {
        current_trace = atomic_fetch_add(&next_trace, 1);
        if (!current_trace) current_trace = atomic_fetch_add(&next_trace, 1);
    }
    return previous;
}
void digit_trace_end(uint64_t previous) { current_trace = previous; }

static int safe(const char *value)
{
    const unsigned char *p = (const unsigned char *)value;
    size_t n = 0;
    if (!p || !*p) return 0;
    while (*p) {
        if (++n > 127 || *p < 33 || *p > 126 || *p == '=' ||
            *p == '\\' || *p == '"') return 0;
        ++p;
    }
    return 1;
}
int digit_trace_record_at(const char *component, const char *operation,
    const char *decision, const char *result, const char *reason,
    const char *file, const char *function, unsigned int line)
{
    char detail[1280];
    int n;
    if (!digit_log_is_open()) return 0;
    n = snprintf(detail, sizeof detail,
        "trace=%" PRIu64 " component=%.127s operation=%.127s decision=%.127s result=%.127s reason=%.127s file=%.127s function=%.127s line=%u",
        current_trace,
        safe(component) ? component : "UNKNOWN",
        safe(operation) ? operation : "UNKNOWN",
        safe(decision) ? decision : "UNKNOWN",
        safe(result) ? result : "UNKNOWN",
        safe(reason) ? reason : "UNKNOWN",
        safe(file) ? file : "UNKNOWN",
        safe(function) ? function : "UNKNOWN",
        safe(file) && safe(function) && line ? line : 0);
    if (n < 0 || (size_t)n >= sizeof detail) return 0;
    return digit_log_event("TRACE", "RECORD", detail) && digit_log_sync();
}
