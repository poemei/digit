#include <string.h>
#include "stnlabz_abi/module.h"

#ifndef FIXTURE_VERSION
#define FIXTURE_VERSION 0
#endif
#ifndef FIXTURE_BAD_QUAL
#define FIXTURE_BAD_QUAL 0
#endif
#ifndef FIXTURE_BAD_START
#define FIXTURE_BAD_START 0
#endif

static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *r)
{
    if (!r) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(r, 0, sizeof *r);
    r->tests_executed = FIXTURE_BAD_QUAL ? 9U : 10U;
    r->tests_passed = r->tests_executed;
    r->negative_test_executed = 1;
    r->negative_test_passed = 1;
    return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host)
{
    (void)host;
    return FIXTURE_BAD_START ? STNLABZ_MODULE_ERR_START_FAILED : STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void) { return STNLABZ_MODULE_OK; }

static const stnlabz_module_descriptor_t module = {
    "fixture", "Core Hotload Test Fixture", 1U, 0U, FIXTURE_VERSION,
    STNLABZ_MODULE_API_MAJOR, STNLABZ_MODULE_API_MINOR,
    qualify, start, stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{
    return &module;
}
