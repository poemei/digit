#ifndef DIGIT_CORE_POLICY_H
#define DIGIT_CORE_POLICY_H
#include "agent_root.h"
typedef enum {
    DIGIT_POLICY_ALLOW = 0,
    DIGIT_POLICY_DENY,
    DIGIT_POLICY_ESCALATE
} digit_policy_decision_t;
typedef struct {
    const char *actor;           /* Already verified by a trusted Core caller. */
    const char *mission;         /* Assigned mission identifier. */
    const char *operation;       /* Operation being requested. */
    const char *module_id;       /* Target module, if applicable. */
    int within_mission;
    int complies_with_policy;
    int instruction_authorized;
    int authority_granted;
    const char *escalation_target; /* Valid next-level contact supplied by operator policy. */
} digit_policy_request_t;
typedef enum {
    DIGIT_ESCALATION_NONE = 0,
    DIGIT_ESCALATION_PENDING_DELIVERY,
    DIGIT_ESCALATION_UNAVAILABLE
} digit_escalation_status_t;
typedef struct {
    digit_policy_decision_t decision;
    const char *policy_id;
    const char *reason;
    digit_escalation_status_t escalation_status; /* No external delivery is implied. */
} digit_policy_result_t;
/* Fail-closed evaluation; writes a synchronized policy decision to the Core log. */
int digit_core_policy_evaluate(const digit_policy_request_t *request,
    digit_policy_result_t *result);
#endif
