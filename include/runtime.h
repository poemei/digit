#ifndef DIGIT_RUNTIME_H
#define DIGIT_RUNTIME_H

typedef enum {
    DIGIT_OPERATIONAL_NORMAL = 0,
    DIGIT_OPERATIONAL_SAFE_MODE = 1,
    DIGIT_OPERATIONAL_COMPANY_PRESERVATION = 2
} digit_operational_state_t;

typedef enum {
    DIGIT_FAULT_AUTHORITY = 0,
    DIGIT_FAULT_INTEGRITY = 1,
    DIGIT_FAULT_SYSTEM = 2,
    DIGIT_FAULT_MISSION = 3,
    DIGIT_FAULT_TRUTHFULNESS = 4,
    DIGIT_FAULT_UNAUTHORIZED_CONDITION = 5
} digit_fault_t;

typedef struct {
    int running;
    digit_operational_state_t operational_state;
    digit_fault_t preservation_cause;
    int preservation_cause_set;
} digit_runtime_t;

/* Core runtime functions do not require any installed modules. */
void digit_runtime_init(digit_runtime_t *runtime);
int digit_runtime_run(digit_runtime_t *runtime);
void digit_runtime_request_stop(void);
int digit_runtime_report_fault(digit_runtime_t *runtime, digit_fault_t fault);
int digit_runtime_human_release(digit_runtime_t *runtime, int authorized_human_release);
digit_operational_state_t digit_runtime_operational_state(const digit_runtime_t *runtime);
int digit_runtime_normal_operations_allowed(const digit_runtime_t *runtime);

#endif
