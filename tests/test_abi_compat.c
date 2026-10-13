#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/abi.h"

static unsigned tests, failed;
static unsigned starts, stops;
static void check(int yes, const char *name)
{
    ++tests;
    if (!yes) { ++failed; fprintf(stderr, "FAIL ABI 1.5: %s\n", name); }
}
static stnlabz_module_result_t qualified(stnlabz_module_qualification_result_t *r)
{
    if (!r) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(r, 0, sizeof *r);
    r->tests_executed = STNLABZ_MODULE_MIN_TESTS;
    r->tests_passed = STNLABZ_MODULE_MIN_TESTS;
    r->negative_test_executed = r->negative_test_passed = 1;
    return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t insufficient(stnlabz_module_qualification_result_t *r)
{
    if (!r) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(r, 0, sizeof *r);
    r->tests_executed = r->tests_passed = STNLABZ_MODULE_MIN_TESTS - 1;
    r->negative_test_executed = r->negative_test_passed = 1;
    return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t started(const stnlabz_module_host_t *h)
{
    (void)h;
    ++starts;
    return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stopped(void)
{
    ++stops;
    return STNLABZ_MODULE_OK;
}
int main(void)
{
    stnlabz_module_registry_t registry;
    stnlabz_module_descriptor_t d;
    const stnlabz_module_record_t *r;
    memset(&d, 0, sizeof d);
    strcpy(d.id, "test-abi");
    strcpy(d.name, "ABI 1.5 test");
    d.version_major = 1;
    d.required_core_api_major = STNLABZ_MODULE_API_MAJOR;
    d.required_core_api_minor = STNLABZ_MODULE_API_MINOR;
    d.qualify = qualified;
    d.start = started;
    d.stop = stopped;
    stnlabz_module_registry_init(&registry);

    check(STNLABZ_MODULE_API_MAJOR == 1 && STNLABZ_MODULE_API_MINOR == 5,
          "common ABI version 1.5");
    check(STNLABZ_MODULE_MIN_TESTS == 10, "common qualification minimum");
    check(stnlabz_module_abi_prepare(&registry, &d) == STNLABZ_MODULE_OK,
          "prepare registered descriptor with qualification callback");
    r = stnlabz_module_registry_find(&registry, "test-abi");
    check(r && r->state == STNLABZ_MODULE_STATE_QUALIFIED,
          "qualification state recorded");
    check(stnlabz_module_registry_activate(&registry, d.id) ==
          STNLABZ_MODULE_ERR_NOT_AUTHORIZED, "authorization gate");
    check(stnlabz_module_abi_authorize_and_activate(&registry, d.id, NULL) ==
          STNLABZ_MODULE_OK, "authorized activation");
    check(starts == 1, "start callback");
    check(stnlabz_module_abi_stop(&registry, d.id) == STNLABZ_MODULE_OK,
          "stop ABI lifecycle");
    check(stops == 1, "stop callback");
    check(stnlabz_module_registry_activate(&registry, d.id) ==
          STNLABZ_MODULE_ERR_NOT_AUTHORIZED, "stop clears authority");
    check(stnlabz_module_abi_prepare_replacement(&registry, d.id) ==
          STNLABZ_MODULE_OK, "unregister old stopped revision");
    check(stnlabz_module_registry_find(&registry, d.id) == NULL,
          "old revision no longer registered");

    d.version_patch = 1;
    d.qualify = insufficient;
    check(stnlabz_module_abi_prepare(&registry, &d) ==
          STNLABZ_MODULE_ERR_QUALIFICATION, "replacement requires ten tests");
    r = stnlabz_module_registry_find(&registry, d.id);
    check(r && r->state == STNLABZ_MODULE_STATE_FAILED,
          "failed qualification cannot become active");
    stnlabz_module_registry_init(&registry);
    d.qualify = qualified;
    d.required_core_api_minor = STNLABZ_MODULE_API_MINOR + 1;
    check(stnlabz_module_abi_prepare(&registry, &d) ==
          STNLABZ_MODULE_ERR_INCOMPATIBLE, "reject unsupported ABI minor");
    stnlabz_module_registry_init(&registry);
    d.required_core_api_minor = STNLABZ_MODULE_API_MINOR;
    check(stnlabz_module_abi_prepare(&registry, &d) ==
          STNLABZ_MODULE_OK, "new revision requalifies");
    printf("Core ABI compatibility tests: %u executed, %u failed\n", tests, failed);
    return failed != 0;
}
