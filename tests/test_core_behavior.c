#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "agent_root.h"
#include "core_policy.h"
#include "logging.h"

static unsigned executed, passed, failed;
#define CHECK(label, expr) do { \
    int ok = !!(expr); \
    ++executed; passed += (unsigned)ok; failed += (unsigned)!ok; \
    printf("%s %s\n", ok ? "PASS" : "FAIL", label); \
} while (0)

static stnlabz_policy_request_t approved_request(void)
{
    stnlabz_policy_request_t r = {
        .actor = "test-operator", .mission = "digit-core-qualification",
        .operation = "module.activate", .module_id = "test-module",
        .within_mission = 1, .complies_with_policy = 1,
        .instruction_authorized = 1, .authority_granted = 1,
        .verification_source = "trusted-test-harness",
        .context_verified = 1
    };
    return r;
}
static int decision(const stnlabz_policy_request_t *req,
                    stnlabz_policy_decision_t expected)
{
    stnlabz_policy_result_t out;
    return stnlabz_core_policy_evaluate(req, &out) &&
           out.decision == expected;
}

int main(void)
{
    char dir[] = "/tmp/digit-core-tests-XXXXXX", logpath[PATH_MAX];
    char modpath[PATH_MAX];
    stnlabz_agent_root_t root;
    stnlabz_policy_request_t req;
    const stnlabz_agent_root_t *identity = stnlabz_agent_root_identity();
    FILE *stream;
    char buffer[8192];
    size_t n;

    if (!mkdtemp(dir)) { perror("mkdtemp"); return 2; }
    if (snprintf(logpath, sizeof logpath, "%s/core.log", dir) >= (int)sizeof logpath ||
        snprintf(modpath, sizeof modpath, "%s/modules.log", dir) >= (int)sizeof modpath) {
        fputs("test path too long\n", stderr); return 2;
    }
    if (!stnlabz_log_open_path(logpath)) {
        fputs("test log initialization failed\n", stderr); return 2;
    }

    /* Actual C Root validator behavior (10 independent cases). */
    CHECK("01 valid compiled Digit Root", stnlabz_agent_root_validate(identity));
    CHECK("02 null Root rejected", !stnlabz_agent_root_validate(NULL));
    root = *identity; root.schema_major++;
    CHECK("03 unsupported Root major rejected", !stnlabz_agent_root_validate(&root));
    root = *identity; root.revision = 0;
    CHECK("04 zero Root revision rejected", !stnlabz_agent_root_validate(&root));
    root = *identity; root.agent_id = "Digit Invalid";
    CHECK("05 malformed agent identifier rejected", !stnlabz_agent_root_validate(&root));
    root = *identity; root.organization_id = "unknown";
    CHECK("06 wrong organization rejected", !stnlabz_agent_root_validate(&root));
    root = *identity; root.orders[0] = "Different order";
    CHECK("07 unauthorized General Order rejected", !stnlabz_agent_root_validate(&root));
    root = *identity; root.core_modification_rule = "Allow unapproved modification";
    CHECK("08 changed Core authorization rule rejected", !stnlabz_agent_root_validate(&root));
    root = *identity; root.traits[0] = "untrusted";
    CHECK("09 changed Root trait rejected", !stnlabz_agent_root_validate(&root));
    root = *identity; root.relation = STNLABZ_ROOT_RELATION_OLDER_SISTER;
    root.related_agent_id = identity->agent_id;
    CHECK("10 self relationship rejected", !stnlabz_agent_root_validate(&root));

    /* Real fail-closed Core policy evaluation and negative cases. */
    req = approved_request();
    CHECK("11 approved verified context allowed", decision(&req, STNLABZ_POLICY_ALLOW));
    req = approved_request(); req.context_verified = 0;
    CHECK("12 unverified context denied", decision(&req, STNLABZ_POLICY_DENY));
    req = approved_request(); req.within_mission = 0;
    CHECK("13 outside mission denied", decision(&req, STNLABZ_POLICY_DENY));
    req = approved_request(); req.complies_with_policy = 0;
    CHECK("14 policy violation denied", decision(&req, STNLABZ_POLICY_DENY));
    req = approved_request(); req.instruction_authorized = 0;
    CHECK("15 unauthorized instruction denied", decision(&req, STNLABZ_POLICY_DENY));
    req = approved_request(); req.authority_granted = 0;
    CHECK("16 absent authority denied", decision(&req, STNLABZ_POLICY_DENY));
    req = approved_request(); req.actor = NULL;
    CHECK("17 missing actor denied", decision(&req, STNLABZ_POLICY_DENY));
    req = approved_request(); req.verification_source = NULL;
    CHECK("18 absent verification source denied", decision(&req, STNLABZ_POLICY_DENY));
    req = approved_request(); req.within_mission = 0;
    req.escalation_target = "director";
    CHECK("19 rejected request with valid escalation target escalates",
          decision(&req, STNLABZ_POLICY_ESCALATE));
    req = approved_request(); req.within_mission = 0;
    req.escalation_target = "director;bad";
    CHECK("20 unsafe escalation target cannot cause escalation",
          decision(&req, STNLABZ_POLICY_DENY));

    if (!stnlabz_log_sync()) { fputs("log sync failed\n", stderr); ++failed; }
    stnlabz_log_close();
    stream = fopen(logpath, "r");
    if (!stream) { fputs("Core audit log missing\n", stderr); ++failed; }
    else {
        n = fread(buffer, 1, sizeof buffer - 1, stream);
        buffer[n] = 0;
        if (!strstr(buffer, "POLICY ALLOW") ||
            !strstr(buffer, "POLICY DENY") ||
            !strstr(buffer, "POLICY ESCALATE")) {
            fputs("Core policy decisions were not all auditable\n", stderr);
            ++failed;
        }
        (void)fclose(stream);
    }
    (void)unlink(logpath);
    (void)unlink(modpath);
    (void)rmdir(dir);
    printf("Core behavior: executed=%u passed=%u failed=%u\n",
           executed, passed, failed);
    return executed >= 20U && passed == executed && failed == 0U ? 0 : 1;
}
