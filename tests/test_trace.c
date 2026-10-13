#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "core_trace.h"
#include "logging.h"

static unsigned checks, failed;
static void check(int ok, const char *reason) {
    ++checks;
    if (!ok) { ++failed; fprintf(stderr, "FAIL trace: %s\n", reason); }
}
int main(int argc, char **argv)
{
    FILE *f;
    char line[1200];
    uint64_t outer, first, second;
    unsigned traced = 0, unknown = 0, nested = 0;
    unsigned i;
    if (argc != 2) return 2;
    if (!digit_log_open_path(argv[1])) return 3;
    check(digit_trace_current() == 0, "initially no correlation");
    outer = digit_trace_begin();
    first = digit_trace_current();
    check(outer == 0 && first != 0, "new correlation id");
    check(DIGIT_TRACE("CORE", "request", "ALLOW", "OK", "authorized"),
          "trace observed decision");
    second = digit_trace_begin();
    check(second == first && digit_trace_current() == first, "nested correlation retained");
    check(DIGIT_TRACE("CORE", "request", "DENY", "REJECTED", "out_of_scope"),
          "correlated rejection");
    digit_trace_end(second);
    check(digit_trace_current() == first, "nested restoration");
    check(DIGIT_TRACE(NULL, NULL, NULL, NULL, NULL), "unknown values recorded");
    digit_trace_end(outer);
    check(digit_trace_current() == 0, "top-level scope cleared");
    for (i = 0; i < 65; ++i) {
        uint64_t previous = digit_trace_begin();
        check(previous == 0 && digit_trace_current() != first, "unique trace id beyond 32");
        check(DIGIT_TRACE("MODULE", "verify", "ALLOW", "OK", "valid"), "trace under load");
        digit_trace_end(previous);
    }
    check(digit_trace_record_at("CORE", "bad", "ALLOW", "OK",
              "test", "no_location", "no_function", 0),
          "unknown line does not fabricate location");
    digit_log_close();
    f = fopen(argv[1], "r");
    if (f) {
        while (fgets(line, sizeof line, f)) {
            if (strstr(line, "TRACE RECORD")) {
                ++traced;
                if (strstr(line, "component=UNKNOWN operation=UNKNOWN") &&
                    strstr(line, "reason=UNKNOWN")) ++unknown;
                if (strstr(line, "decision=DENY result=REJECTED") &&
                    strstr(line, "trace=")) ++nested;
            }
        }
        fclose(f);
    }
    check(traced == 69, "all traces auditable");
    check(unknown == 1, "unknown evidence explicit");
    check(nested == 1, "negative decision preserved");
    printf("Core trace tests: %u executed, %u failed\n", checks, failed);
    return failed != 0;
}
