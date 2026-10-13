#include <stdio.h>
#include <string.h>
#include "core_activation.h"
#include "core_policy.h"
#include "core_discovery.h"
#include "logging.h"
static unsigned checks, failures, starts, stops;
static int start_failure, stop_failure;
static void check(int ok, const char *reason) {
    ++checks;
    if (!ok) { ++failures; fprintf(stderr, "FAIL activation: %s\n", reason); }
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q) {
    memset(q, 0, sizeof *q);
    q->tests_executed = q->tests_passed = STNLABZ_MODULE_MIN_TESTS;
    q->negative_test_executed = q->negative_test_passed = 1;
    return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host) {
    (void)host;
    ++starts;
    return start_failure ? STNLABZ_MODULE_ERR_START_FAILED : STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void) {
    ++stops;
    return stop_failure ? STNLABZ_MODULE_ERR_STOP_FAILED : STNLABZ_MODULE_OK;
}
int main(int argc, char **argv) {
    stnlabz_module_registry_t reg;
    stnlabz_module_loader_t loader;
    stnlabz_module_descriptor_t d;
    const stnlabz_module_record_t *r;
    FILE *f;
    char line[1024];
    unsigned i, activated = 0, stopped = 0, denials = 0;
    digit_policy_request_t policy = { "operator", "core-validation",
        "module.activate", NULL, 1, 1, 1, 1, NULL, "core-test-fixture", 1 };
    if (argc != 2) return 2;
    if (!digit_log_open_path(argv[1])) return 3;
    stnlabz_module_registry_init(&reg);
    stnlabz_module_loader_init(&loader);
    memset(&d, 0, sizeof d);
    strcpy(d.name, "Activation test");
    d.required_core_api_major = STNLABZ_MODULE_API_MAJOR;
    d.required_core_api_minor = STNLABZ_MODULE_API_MINOR;
    d.qualify = qualify; d.start = start; d.stop = stop;
    for (i = 0; i < 48; ++i) {
        snprintf(d.id, sizeof d.id, "active-%03u", i);
        check(stnlabz_module_registry_discover(&reg, &d) == STNLABZ_MODULE_OK, "register");
        check(stnlabz_module_registry_verify(&reg, d.id) == STNLABZ_MODULE_OK, "verify");
    }
    policy.module_id = "active-000";
    check(digit_core_authorize(&reg, "active-000", &(digit_policy_request_t){ "operator", "core-validation", "module.activate", "active-000", 1, 1, 1, 1, NULL, "core-test-fixture", 1 }) == STNLABZ_MODULE_ERR_NOT_QUALIFIED,
          "unqualified authorization denied");
    check(digit_core_activate(&reg, "active-000", NULL, &policy) == STNLABZ_MODULE_ERR_NOT_QUALIFIED,
          "unqualified activation denied");
    check(starts == 0, "unauthorized callback never executed");
    check(digit_core_qualify(&reg), "batch qualification");
    check(digit_core_activate(&reg, "active-000", NULL, &policy) == STNLABZ_MODULE_ERR_NOT_AUTHORIZED,
          "qualified alone does not activate");
    check(starts == 0, "qualified callback not started");
    policy.context_verified = 0;
    check(digit_core_authorize(&reg, "active-000", &policy) ==
          STNLABZ_MODULE_ERR_NOT_AUTHORIZED,
          "forged caller flags cannot grant authorization");
    check(digit_core_activate(&reg, "active-000", NULL, &policy) ==
          STNLABZ_MODULE_ERR_NOT_AUTHORIZED,
          "forged caller flags cannot start qualified module");
    check(starts == 0, "forged authority invokes no callback");
    policy.context_verified = 1;
    for (i = 0; i < 48; ++i) {
        char id[64];
        snprintf(id, sizeof id, "active-%03u", i);
        check((policy.module_id = id, digit_core_authorize(&reg, id, &policy)) == STNLABZ_MODULE_OK, "explicit authorization");
        policy.module_id = id;
        check(digit_core_activate(&reg, id, NULL, &policy) == STNLABZ_MODULE_OK, "explicit activation");
    }
    check(starts == 48, "48 starts executed exactly once");
    policy.module_id = "active-000";
    policy.context_verified = 0;
    check(digit_core_activate(&reg, "active-000", NULL, &policy) == STNLABZ_MODULE_ERR_NOT_QUALIFIED,
          "active module remains non-reentrant with forged claims");
    policy.context_verified = 1;
    check(digit_core_activate(&reg, "active-000", NULL, &policy) == STNLABZ_MODULE_ERR_NOT_QUALIFIED,
          "duplicate activation denied");
    check(starts == 48, "duplicate start callback suppressed");
    check(digit_core_stop_all(&reg), "stop all active modules");
    check(stops == 48, "48 stop callbacks executed");
    for (i = 0; i < 48; ++i) {
        char id[64];
        snprintf(id, sizeof id, "active-%03u", i);
        r = stnlabz_module_registry_find(&reg, id);
        check(r && r->state == STNLABZ_MODULE_STATE_STOPPED &&
              !r->activation_authorized &&
              r->qualification.tests_passed == STNLABZ_MODULE_MIN_TESTS,
              "stopped state retains qualification without authority");
    }
    check(digit_core_activate(&reg, "active-000", NULL, &policy) == STNLABZ_MODULE_ERR_NOT_AUTHORIZED,
          "stopped module requires reauthorization");
    check((policy.module_id = "active-000", digit_core_authorize(&reg, "active-000", &policy)) == STNLABZ_MODULE_OK,
          "explicit reauthorization");
    policy.module_id = "active-000";
    check(digit_core_activate(&reg, "active-000", NULL, &policy) == STNLABZ_MODULE_OK,
          "explicit reactivation");
    check(digit_core_stop_all(&reg), "reauthorized module stopped");
    check(stops == 49, "reactivation stop callback ran");
    digit_core_discovery_cleanup(&reg, &loader);
    check(reg.count == 0 && loader.count == 0, "all stopped modules cleaned up");
    strcpy(d.id, "start-fails");
    check(stnlabz_module_registry_discover(&reg, &d) == STNLABZ_MODULE_OK, "start failure setup");
    check(stnlabz_module_registry_verify(&reg, d.id) == STNLABZ_MODULE_OK, "start failure verify");
    check(digit_core_qualify(&reg), "start failure qualify");
    check((policy.module_id = d.id, digit_core_authorize(&reg, d.id, &policy)) == STNLABZ_MODULE_OK, "start failure authorize");
    start_failure = 1;
    policy.module_id = d.id;
    check(digit_core_activate(&reg, d.id, NULL, &policy) == STNLABZ_MODULE_ERR_START_FAILED,
          "start failure propagated");
    r = stnlabz_module_registry_find(&reg, d.id);
    check(r && r->state == STNLABZ_MODULE_STATE_FAILED && !r->activation_authorized,
          "start failure revokes authority");
    start_failure = 0;
    digit_core_discovery_cleanup(&reg, &loader);
    strcpy(d.id, "stop-fails");
    check(stnlabz_module_registry_discover(&reg, &d) == STNLABZ_MODULE_OK, "stop failure setup");
    check(stnlabz_module_registry_verify(&reg, d.id) == STNLABZ_MODULE_OK, "stop failure verify");
    check(digit_core_qualify(&reg), "stop failure qualify");
    check((policy.module_id = d.id, digit_core_authorize(&reg, d.id, &policy)) == STNLABZ_MODULE_OK, "stop failure authorize");
    check(digit_core_activate(&reg, d.id, NULL, &policy) == STNLABZ_MODULE_OK, "stop failure activated");
    stop_failure = 1;
    check(!digit_core_stop_all(&reg), "stop failure reported");
    r = stnlabz_module_registry_find(&reg, d.id);
    check(r && r->state == STNLABZ_MODULE_STATE_FAILED && !r->activation_authorized,
          "failed stop cannot remain ACTIVE");
    stop_failure = 0;
    /* This is a mock callback; no runtime is retained. */
    digit_core_discovery_cleanup(&reg, &loader);
    digit_log_close();
    char module_path[2048];
    snprintf(module_path,sizeof module_path,"%s",argv[1]);
    char *slash=strrchr(module_path,'/');
    if(slash)strcpy(slash+1,"modules.log");
    f = fopen(module_path, "r");
    if (f) {
        while (fgets(line, sizeof line, f)) {
            if (strstr(line, "MODULE HOTLOAD_ACTIVE")) ++activated;
            if (strstr(line, "MODULE STOP_PASS")) ++stopped;
            if (strstr(line, "MODULE ACTIVATE_REJECT") ||
                strstr(line, "MODULE AUTHORIZE_REJECT")) ++denials;
        }
        fclose(f);
    }
    check(activated == 50, "activation results audited");
    check(stopped == 49, "successful stops audited");
    check(denials >= 4, "denials and failure events audited");
    printf("Core activation tests: %u executed, %u failed\n", checks, failures);
    return failures != 0;
}
