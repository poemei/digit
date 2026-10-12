#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <time.h>
#include "runtime.h"

/* Core lifecycle; no module manager or hotload dependency. */
static volatile sig_atomic_t digit_stop_requested = 0;

static void digit_runtime_signal_handler(int signal_number)
{
    (void)signal_number;
    digit_stop_requested = 1;
}
void digit_runtime_request_stop(void) { digit_stop_requested = 1; }

void digit_runtime_init(digit_runtime_t *runtime)
{
    if (!runtime) return;
    runtime->running = 0;
    runtime->operational_state = DIGIT_OPERATIONAL_NORMAL;
    runtime->preservation_cause = DIGIT_FAULT_SYSTEM;
    runtime->preservation_cause_set = 0;
}
int digit_runtime_report_fault(digit_runtime_t *runtime, digit_fault_t fault)
{
    if (!runtime) return 0;
    /* Qualifying Core faults enter preservation; module failures do not imply one. */
    if (fault < DIGIT_FAULT_AUTHORITY || fault > DIGIT_FAULT_UNAUTHORIZED_CONDITION) return 0;
    runtime->operational_state = DIGIT_OPERATIONAL_SAFE_MODE;
    runtime->preservation_cause = fault;
    runtime->preservation_cause_set = 1;
    runtime->operational_state = DIGIT_OPERATIONAL_COMPANY_PRESERVATION;
    return 1;
}
int digit_runtime_human_release(digit_runtime_t *runtime, int authorized_human_release)
{
    if (!runtime || !authorized_human_release ||
        runtime->operational_state != DIGIT_OPERATIONAL_COMPANY_PRESERVATION)
        return 0;
    runtime->operational_state = DIGIT_OPERATIONAL_NORMAL;
    runtime->preservation_cause_set = 0;
    return 1;
}
digit_operational_state_t digit_runtime_operational_state(const digit_runtime_t *runtime)
{
    return runtime ? runtime->operational_state : DIGIT_OPERATIONAL_COMPANY_PRESERVATION;
}
int digit_runtime_normal_operations_allowed(const digit_runtime_t *runtime)
{
    return runtime && runtime->operational_state == DIGIT_OPERATIONAL_NORMAL;
}
int digit_runtime_run(digit_runtime_t *runtime)
{
    struct sigaction action;
    struct sigaction previous_int, previous_term;
    struct timespec delay = { 1, 0 };
    if (!runtime || runtime->running) return 0;
    action.sa_handler = digit_runtime_signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction(SIGINT, &action, &previous_int) != 0) return 0;
    if (sigaction(SIGTERM, &action, &previous_term) != 0) {
        (void)sigaction(SIGINT, &previous_int, NULL);
        return 0;
    }
    digit_stop_requested = 0;
    runtime->running = 1;
    while (!digit_stop_requested) {
        struct timespec remaining = delay;
        while (nanosleep(&remaining, &remaining) != 0) {
            if (errno == EINTR) {
                if (digit_stop_requested) break;
                continue;
            }
            (void)digit_runtime_report_fault(runtime, DIGIT_FAULT_SYSTEM);
            runtime->running = 0;
            (void)sigaction(SIGINT, &previous_int, NULL);
            (void)sigaction(SIGTERM, &previous_term, NULL);
            return 0;
        }
        /* Core runs independently; qualified modules may attach in a later phase. */
    }
    runtime->running = 0;
    (void)sigaction(SIGINT, &previous_int, NULL);
    (void)sigaction(SIGTERM, &previous_term, NULL);
    return 1;
}
