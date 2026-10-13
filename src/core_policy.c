#include <stdio.h>
#include <string.h>
#include "core_policy.h"
#include "logging.h"

static int valid_target(const char *s)
{
    size_t i;
    if (!s || !s[0]) return 0;
    for (i = 0; i < STNLABZ_ROOT_TEXT_MAX && s[i]; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' ||
              c == '.' || c == '@' || c == '/')) return 0;
    }
    return i > 0 && i < STNLABZ_ROOT_TEXT_MAX;
}
static int valid(const char *s)
{
    return s && s[0] && strlen(s) < STNLABZ_ROOT_TEXT_MAX;
}
int digit_core_policy_evaluate(const digit_policy_request_t *request,
    digit_policy_result_t *result)
{
    char detail[1024];
    int written;
    const char *decision;
    const stnlabz_agent_root_t *root = stnlabz_agent_root_identity();
    if (!result || !digit_log_is_open()) return 0;
    result->decision = DIGIT_POLICY_DENY;
    result->escalation_status = DIGIT_ESCALATION_NONE;
    result->policy_id = "CORE.GO2";
    result->reason = "missing_or_invalid_verified_context";
    if (!stnlabz_agent_root_validate(root)) {
        result->policy_id = "CORE.ROOT";
        result->reason = "invalid_agent_root";
    } else if (request && valid(request->actor) && valid(request->mission) &&
               valid(request->operation) && valid(request->module_id) &&
               valid(request->verification_source) &&
               request->context_verified == 1) {
        if (!request->within_mission) {
            result->policy_id = "CORE.GO1";
            result->reason = "outside_assigned_mission";
        } else if (!request->complies_with_policy || !request->instruction_authorized) {
            result->policy_id = "CORE.GO2";
            result->reason = "policy_or_instruction_not_authorized";
        } else if (!request->authority_granted) {
            result->policy_id = "CORE.GO3";
            result->reason = "outside_delegated_authority";
        } else {
            result->decision = DIGIT_POLICY_ALLOW;
            result->policy_id = "CORE.GO1_GO2_GO3";
            result->reason = "within_mission_policy_and_authority";
        }
        if (result->decision != DIGIT_POLICY_ALLOW &&
            valid_target(request->escalation_target)) {
            result->decision = DIGIT_POLICY_ESCALATE;
        }
        if (result->decision == DIGIT_POLICY_ESCALATE)
            result->escalation_status = DIGIT_ESCALATION_PENDING_DELIVERY;
        else if (result->decision != DIGIT_POLICY_ALLOW &&
                 !request->authority_granted)
            result->escalation_status = DIGIT_ESCALATION_UNAVAILABLE;
    }
    decision = result->decision == DIGIT_POLICY_ALLOW ? "ALLOW" :
        result->decision == DIGIT_POLICY_ESCALATE ? "ESCALATE" : "DENY";
    written = snprintf(detail, sizeof detail,
        "actor=%.127s mission=%.127s operation=%.127s module=%.63s decision=%s policy=%s reason=%s escalation=%.127s",
        request && valid(request->actor) ? request->actor : "unknown",
        request && valid(request->mission) ? request->mission : "unknown",
        request && valid(request->operation) ? request->operation : "unknown",
        request && valid(request->module_id) ? request->module_id : "unknown",
        decision, result->policy_id, result->reason,
        request && valid(request->escalation_target) ?
            request->escalation_target : "unknown");
    if (written < 0 || (size_t)written >= sizeof detail) return 0;
    if (!digit_log_event("POLICY", decision, detail) || !digit_log_sync())
        return 0;
    if (result->escalation_status != DIGIT_ESCALATION_NONE) {
        written = snprintf(detail, sizeof detail,
            "actor=%.127s mission=%.127s operation=%.127s module=%.63s policy=%s reason=%s recipient=%.127s status=%s execution=prohibited",
            request && valid(request->actor) ? request->actor : "unknown",
            request && valid(request->mission) ? request->mission : "unknown",
            request && valid(request->operation) ? request->operation : "unknown",
            request && valid(request->module_id) ? request->module_id : "unknown",
            result->policy_id, result->reason,
            request && valid_target(request->escalation_target) ?
                request->escalation_target : "unknown",
            result->escalation_status == DIGIT_ESCALATION_PENDING_DELIVERY ?
                "pending_delivery" : "recipient_unavailable");
        if (written < 0 || (size_t)written >= sizeof detail) return 0;
        if (!digit_log_event("ESCALATION", "REPORT", detail) ||
            !digit_log_sync()) return 0;
    }
    return 1;
}
