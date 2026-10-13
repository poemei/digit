#include <stdio.h>
#include <string.h>
#include "core_policy.h"
#include "core_activation.h"
#include "logging.h"

static unsigned assertions, failures;
static void check(int condition, const char *reason)
{
    ++assertions;
    if (!condition) { ++failures; fprintf(stderr, "FAIL policy: %s\n", reason); }
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
    memset(q, 0, sizeof(*q));
    q->tests_executed = q->tests_passed = STNLABZ_MODULE_MIN_TESTS;
    q->negative_test_executed = q->negative_test_passed = 1;
    return STNLABZ_MODULE_OK;
}
int main(int argc, char **argv)
{
    stnlabz_module_registry_t reg;
    stnlabz_module_descriptor_t descriptor;
    digit_policy_request_t request = {
        "operator", "mission-core", "module.activate", "policy-000",
        1, 1, 1, 1, NULL, "core-test-fixture", 1
    };
    digit_policy_result_t result;
    unsigned i, allows = 0, denies = 0, escalations = 0, reports_pending = 0, reports_unavailable = 0;
    FILE *f;
    char line[1024];
    if (argc != 2) return 2;
    if (!digit_log_open_path(argv[1])) return 3;
    stnlabz_module_registry_init(&reg);
    check(digit_core_policy_evaluate(&request, &result) &&
          result.decision == DIGIT_POLICY_ALLOW, "valid authorized request");
    request.context_verified = 0;
    check(digit_core_policy_evaluate(&request, &result) &&
          result.decision == DIGIT_POLICY_DENY,
          "forged authority flags without verified context denied");
    request.context_verified = 1;
    request.verification_source = NULL;
    check(digit_core_policy_evaluate(&request, &result) &&
          result.decision == DIGIT_POLICY_DENY,
          "missing provenance source denied");
    request.verification_source = "core-test-fixture";
    request.within_mission = 0;
    check(digit_core_policy_evaluate(&request, &result) &&
          result.decision == DIGIT_POLICY_DENY &&
          !strcmp(result.policy_id, "CORE.GO1"), "mission restriction");
    request.within_mission = 1;
    request.complies_with_policy = 0;
    check(digit_core_policy_evaluate(&request, &result) &&
          result.decision == DIGIT_POLICY_DENY &&
          !strcmp(result.policy_id, "CORE.GO2"), "policy restriction");
    request.complies_with_policy = 1;
    request.instruction_authorized = 0;
    check(digit_core_policy_evaluate(&request, &result) &&
          result.decision == DIGIT_POLICY_DENY, "unauthorized instruction");
    request.instruction_authorized = 1;
    request.authority_granted = 0;
    check(digit_core_policy_evaluate(&request, &result) &&
          result.decision == DIGIT_POLICY_DENY &&
          !strcmp(result.policy_id, "CORE.GO3"), "missing authority");
    request.escalation_target = "operator";
    check(digit_core_policy_evaluate(&request, &result) &&
          result.decision == DIGIT_POLICY_ESCALATE &&
          result.escalation_status == DIGIT_ESCALATION_PENDING_DELIVERY,
          "report beyond authority is pending, not delivered");
    request.escalation_target = "invalid recipient";
    check(digit_core_policy_evaluate(&request, &result) &&
          result.decision == DIGIT_POLICY_DENY &&
          result.escalation_status == DIGIT_ESCALATION_UNAVAILABLE,
          "invalid recipient denies and records unavailable");
    request.escalation_target = NULL;
    check(digit_core_policy_evaluate(&request, &result) &&
          result.decision == DIGIT_POLICY_DENY &&
          result.escalation_status == DIGIT_ESCALATION_UNAVAILABLE,
          "missing recipient denies and records unavailable");
    request.escalation_target = "operator";
    request.authority_granted = 1;
    request.actor = NULL;
    check(digit_core_policy_evaluate(&request, &result) &&
          result.decision == DIGIT_POLICY_DENY, "missing verified actor");
    request.actor = "operator";
    request.escalation_target = NULL;
    check(digit_core_policy_evaluate(NULL, &result) &&
          result.decision == DIGIT_POLICY_DENY, "missing context");
    memset(&descriptor, 0, sizeof descriptor);
    strcpy(descriptor.name, "Policy fixture");
    descriptor.required_core_api_major = STNLABZ_MODULE_API_MAJOR;
    descriptor.required_core_api_minor = STNLABZ_MODULE_API_MINOR;
    descriptor.qualify = qualify;
    for (i = 0; i < 48; ++i) {
        snprintf(descriptor.id, sizeof descriptor.id, "policy-%03u", i);
        check(stnlabz_module_registry_discover(&reg, &descriptor) ==
              STNLABZ_MODULE_OK, "register");
        check(stnlabz_module_registry_verify(&reg, descriptor.id) ==
              STNLABZ_MODULE_OK, "verify");
        check(stnlabz_module_registry_qualify(&reg, descriptor.id) ==
              STNLABZ_MODULE_OK, "qualify");
    }
    check(reg.count == 48, "dynamic registry");
    request.context_verified = 0;
    check(digit_core_authorize(&reg, "policy-000", &request) ==
          STNLABZ_MODULE_ERR_NOT_AUTHORIZED, "unverified provenance cannot authorize");
    check(!reg.modules[0].activation_authorized,
          "unverified provenance leaves authorization unset");
    request.context_verified = 1;
    request.authority_granted = 0;
    check(digit_core_authorize(&reg, "policy-000", &request) ==
          STNLABZ_MODULE_ERR_NOT_AUTHORIZED, "Core blocks unauthorized activation");
    check(!reg.modules[0].activation_authorized, "authorization absent after denial");
    request.authority_granted = 1;
    request.operation = "module.stop";
    check(digit_core_authorize(&reg, "policy-000", &request) ==
          STNLABZ_MODULE_ERR_NOT_AUTHORIZED, "operation scope enforced");
    request.operation = "module.activate";
    check(digit_core_authorize(&reg, "policy-001", &request) ==
          STNLABZ_MODULE_ERR_NOT_AUTHORIZED, "module target scope enforced");
    check(digit_core_authorize(&reg, "policy-000", NULL) ==
          STNLABZ_MODULE_ERR_NOT_AUTHORIZED, "no default authorization");
    for (i = 0; i < 48; ++i) {
        char id[64];
        snprintf(id, sizeof id, "policy-%03u", i);
        request.module_id = id;
        check(digit_core_authorize(&reg, id, &request) ==
              STNLABZ_MODULE_OK, "allowed explicit authorization");
    }
    for (i = 0; i < 48; ++i)
        check(reg.modules[i].activation_authorized &&
              reg.modules[i].state == STNLABZ_MODULE_STATE_QUALIFIED,
              "authority granted without activation");
    /* A raw ABI registry grant cannot substitute for Core policy at execution. */
    request.module_id = "policy-000";
    check(digit_core_activate(&reg, "policy-000", NULL, NULL) ==
          STNLABZ_MODULE_ERR_NOT_AUTHORIZED, "missing execution authority denied");
    check(reg.modules[0].state == STNLABZ_MODULE_STATE_QUALIFIED,
          "denied execution leaves module inactive");
    request.authority_granted = 0;
    check(digit_core_activate(&reg, "policy-000", NULL, &request) ==
          STNLABZ_MODULE_ERR_NOT_AUTHORIZED, "revoked authority denied at activation");
    request.authority_granted = 1;
    request.module_id = "policy-001";
    check(digit_core_activate(&reg, "policy-000", NULL, &request) ==
          STNLABZ_MODULE_ERR_NOT_AUTHORIZED, "mismatched module denied at activation");
    request.module_id = "policy-000";
    request.operation = "module.stop";
    check(digit_core_activate(&reg, "policy-000", NULL, &request) ==
          STNLABZ_MODULE_ERR_NOT_AUTHORIZED, "mismatched operation denied at activation");
    request.operation = "module.activate";
    check(digit_core_activate(&reg, "policy-000", NULL, &request) ==
          STNLABZ_MODULE_OK, "authorized activation after recheck");
    check(reg.modules[0].state == STNLABZ_MODULE_STATE_ACTIVE,
          "valid policy enables only intended module");
    check(stnlabz_module_registry_stop(&reg, "policy-000") ==
          STNLABZ_MODULE_OK, "authorized mock activation stopped");
    check(!reg.modules[0].activation_authorized,
          "stop revokes authorization");
    digit_log_close();
    f = fopen(argv[1], "r");
    if (f) {
        while (fgets(line, sizeof line, f)) {
            if (strstr(line, " POLICY ALLOW ")) ++allows;
            if (strstr(line, " POLICY DENY ")) ++denies;
            if (strstr(line, " POLICY ESCALATE ")) ++escalations;
            if (strstr(line, " ESCALATION REPORT ") &&
                strstr(line, "status=pending_delivery") &&
                strstr(line, "execution=prohibited")) ++reports_pending;
            if (strstr(line, " ESCALATION REPORT ") &&
                strstr(line, "status=recipient_unavailable") &&
                strstr(line, "execution=prohibited")) ++reports_unavailable;
        }
        fclose(f);
    }
    check(allows >= 49 && denies >= 8 && escalations == 1,
          "policy outcomes synchronized to audit");
    check(reports_pending == 1 && reports_unavailable >= 3,
          "Core escalation reporting status persisted without delivery claim");
    stnlabz_module_registry_destroy(&reg);
    printf("Core policy tests: %u executed, %u failed\n", assertions, failures);
    return failures != 0;
}
