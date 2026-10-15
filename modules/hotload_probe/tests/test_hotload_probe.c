#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"

const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);

static unsigned checks, failures;
static void check(int value, const char *label)
{
    checks++;
    if (!value) { failures++; fprintf(stderr, "FAIL %s\n", label); }
}
static int log_message(const char *message)
{
    return message != NULL && message[0] != 0;
}
int main(void)
{
    stnlabz_module_qualification_result_t result;
    stnlabz_module_host_t host = {0};
    const stnlabz_module_descriptor_t *d = stnlabz_module_get_descriptor();
    host.send_message = log_message;
    check(d != NULL, "descriptor");
    if (!d) return 1;
    check(!strcmp(d->id, "hotload_probe"), "identity");
    check(d->version_major == 0, "major");
    check(d->version_minor == 1, "minor");
    check(d->version_patch == HOTLOAD_PROBE_PATCH, "patch");
    check(d->required_core_api_major == STNLABZ_MODULE_API_MAJOR, "abi major");
    check(d->required_core_api_minor == STNLABZ_MODULE_API_MINOR, "abi minor");
    check(d->qualify(NULL) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT, "null qualification");
    check(d->qualify(&result) == STNLABZ_MODULE_OK, "qualification");
    check(result.tests_executed >= STNLABZ_MODULE_MIN_TESTS, "minimum qualification");
    check(result.tests_passed == result.tests_executed && !result.tests_failed, "passing qualification");
    check(result.negative_test_executed && result.negative_test_passed, "negative qualification");
    check(d->start(NULL) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT, "null host");
    check(d->stop() == STNLABZ_MODULE_ERR_INVALID_STATE, "inactive stop");
    check(d->start(&host) == STNLABZ_MODULE_OK, "start");
    check(d->start(&host) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT, "double start");
    check(d->stop() == STNLABZ_MODULE_OK, "stop");
    check(d->stop() == STNLABZ_MODULE_ERR_INVALID_STATE, "double stop");
    printf("HOTLOAD_PROBE TEST: %u checks, %u failed\n", checks, failures);
    return failures ? 1 : 0;
}
