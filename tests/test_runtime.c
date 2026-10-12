#include <stdio.h>
#include "runtime.h"

static unsigned tests, failed;
static void check(int truth, const char *name)
{
    ++tests;
    if (!truth) { ++failed; fprintf(stderr, "FAIL runtime: %s\n", name); }
}
int main(void)
{
    digit_runtime_t runtime;
    digit_runtime_init(&runtime);
    check(!runtime.running, "initially stopped");
    check(digit_runtime_operational_state(&runtime) == DIGIT_OPERATIONAL_NORMAL, "initial normal state");
    check(digit_runtime_normal_operations_allowed(&runtime), "normal operations permitted");
    check(!digit_runtime_report_fault(NULL, DIGIT_FAULT_SYSTEM), "null runtime denied");
    check(!digit_runtime_report_fault(&runtime, (digit_fault_t)99), "invalid fault denied");
    check(digit_runtime_report_fault(&runtime, DIGIT_FAULT_INTEGRITY), "Core fault recorded");
    check(runtime.preservation_cause_set && runtime.preservation_cause == DIGIT_FAULT_INTEGRITY, "fault evidence retained");
    check(digit_runtime_operational_state(&runtime) == DIGIT_OPERATIONAL_COMPANY_PRESERVATION, "preservation entered");
    check(!digit_runtime_normal_operations_allowed(&runtime), "normal operations stopped");
    check(!digit_runtime_human_release(&runtime, 0), "unauthorized release denied");
    check(!digit_runtime_normal_operations_allowed(&runtime), "denied release preserves state");
    check(digit_runtime_human_release(&runtime, 1), "authorized release accepted");
    check(digit_runtime_normal_operations_allowed(&runtime), "normal state restored");
    check(!digit_runtime_human_release(&runtime, 1), "release without preservation denied");
    check(digit_runtime_operational_state(NULL) == DIGIT_OPERATIONAL_COMPANY_PRESERVATION, "null state fails closed");
    printf("Core runtime tests: %u executed, %u failed\n", tests, failed);
    return failed != 0;
}
