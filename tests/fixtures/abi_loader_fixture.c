#include "stnlabz_abi/module.h"
#include <string.h>
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
    if (!q) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(q, 0, sizeof *q);
    q->tests_executed = STNLABZ_MODULE_MIN_TESTS;
    q->tests_passed = STNLABZ_MODULE_MIN_TESTS;
    q->negative_test_executed = q->negative_test_passed = 1;
    return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor = {
    TEST_MODULE_ID, "ABI Loader Fixture", 1, 0, 0,
    STNLABZ_MODULE_API_MAJOR, STNLABZ_MODULE_API_MINOR,
    qualify, NULL, NULL
};
#ifdef TEST_EXPORT
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{
    return &descriptor;
}
#else
const stnlabz_module_descriptor_t *legacy_get_descriptor(void)
{
    return &descriptor;
}
#endif
