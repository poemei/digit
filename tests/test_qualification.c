#include <stdio.h>
#include <string.h>
#include "core_discovery.h"
#include "logging.h"

static unsigned checks, failures, callbacks, starts;
static int mode;
static void check(int ok, const char *what)
{
    ++checks;
    if (!ok) { ++failures; fprintf(stderr, "FAIL qualification: %s\n", what); }
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
    ++callbacks;
    memset(q, 0, sizeof *q);
    q->tests_executed = q->tests_passed =
        mode == 1 ? STNLABZ_MODULE_MIN_TESTS - 1 : STNLABZ_MODULE_MIN_TESTS;
    q->negative_test_executed = 1;
    q->negative_test_passed = mode == 2 ? 0 : 1;
    if (mode == 3) { q->tests_failed = 1; --q->tests_passed; }
    return mode == 4 ? STNLABZ_MODULE_ERR_QUALIFICATION : STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host)
{
    (void)host;
    ++starts;
    return STNLABZ_MODULE_OK;
}
int main(int argc, char **argv)
{
    stnlabz_module_registry_t registry;
    stnlabz_module_loader_t loader;
    stnlabz_module_descriptor_t d;
    const stnlabz_module_record_t *r;
    FILE *log;
    char line[1024];
    unsigned i, passed_log = 0, rejected_log = 0;
    if (argc != 2) return 2;
    stnlabz_module_registry_init(&registry);
    stnlabz_module_loader_init(&loader);
    if (!digit_log_open_path(argv[1])) return 3;
    memset(&d, 0, sizeof d);
    strcpy(d.name, "Qualification fixture");
    d.required_core_api_major = STNLABZ_MODULE_API_MAJOR;
    d.required_core_api_minor = STNLABZ_MODULE_API_MINOR;
    d.qualify = qualify;
    d.start = start;
    for (i = 0; i < 64; ++i) {
        snprintf(d.id, sizeof d.id, "qual-%03u", i);
        check(stnlabz_module_registry_discover(&registry, &d) == STNLABZ_MODULE_OK,
              "registration");
        check(stnlabz_module_registry_verify(&registry, d.id) == STNLABZ_MODULE_OK,
              "verification");
    }
    check(registry.count == 64 && registry.capacity >= 64, "dynamic registry size");
    /* One callback is shared by fixtures; select failure modes through a separate
       registry in each case below. The 64-candidate batch is all successful. */
    mode = 0;
    check(digit_core_qualify(&registry), "64 candidate qualification");
    check(callbacks == 64, "all callbacks invoked");
    for (i = 0; i < 64; ++i) {
        char id[64];
        snprintf(id, sizeof id, "qual-%03u", i);
        r = stnlabz_module_registry_find(&registry, id);
        check(r && r->state == STNLABZ_MODULE_STATE_QUALIFIED &&
              r->qualification.tests_executed == STNLABZ_MODULE_MIN_TESTS &&
              r->qualification.negative_test_passed &&
              !r->activation_authorized, "qualification evidence and no authority");
    }
    check(starts == 0, "no start callback");
    check(stnlabz_module_registry_activate(&registry, "qual-000") ==
          STNLABZ_MODULE_ERR_NOT_AUTHORIZED, "activation gate intact");
    check(digit_core_qualify(&registry) && callbacks == 64,
          "already qualified candidates not rerun");
    digit_core_discovery_cleanup(&registry, &loader);
    check(registry.count == 0 && registry.modules == NULL, "qualified cleanup");
    for (i = 1; i <= 4; ++i) {
        mode = (int)i;
        snprintf(d.id, sizeof d.id, "failed-%03u", i);
        check(stnlabz_module_registry_discover(&registry, &d) == STNLABZ_MODULE_OK,
              "failed fixture discovered");
        check(stnlabz_module_registry_verify(&registry, d.id) == STNLABZ_MODULE_OK,
              "failed fixture verified");
        check(digit_core_qualify(&registry), "failure does not abort Core");
        r = stnlabz_module_registry_find(&registry, d.id);
        check(r && r->state == STNLABZ_MODULE_STATE_FAILED &&
              !r->activation_authorized, "failure is gated");
        check(stnlabz_module_registry_activate(&registry, d.id) ==
              STNLABZ_MODULE_ERR_NOT_QUALIFIED, "failed activation prohibited");
        digit_core_discovery_cleanup(&registry, &loader);
    }
    check(starts == 0, "failed modules never started");
    digit_log_close();
    char module_path[2048];
    snprintf(module_path,sizeof module_path,"%s",argv[1]);
    char *slash=strrchr(module_path,'/');
    if(slash)strcpy(slash+1,"modules.log");
    log = fopen(module_path, "r");
    if (log) {
        while (fgets(line, sizeof line, log)) {
            if (strstr(line, "MODULE QUALIFICATION_GREEN") &&
                strstr(line, "sequence=") && strstr(line, "module=qual-"))
                ++passed_log;
            if (strstr(line, "MODULE QUALIFICATION_FAILED") &&
                strstr(line, "state=FAILED")) ++rejected_log;
        }
        fclose(log);
    }
    check(passed_log == 64, "each successful qualification audited");
    check(rejected_log == 4, "each failure audited");
    printf("Core qualification tests: %u executed, %u failed\n", checks, failures);
    return failures != 0;
}
