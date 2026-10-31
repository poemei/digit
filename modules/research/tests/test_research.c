#include <stdio.h>
#include "stnlabz_abi/module.h"

/* Exported entry point of the Research module under test. */
extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);

int main(void)
{
    const stnlabz_module_descriptor_t *m = stnlabz_module_get_descriptor();
    stnlabz_module_qualification_result_t q = {0};
    int ok = m && m->qualify &&
             m->qualify(&q) == STNLABZ_MODULE_OK &&
             q.tests_executed >= STNLABZ_MODULE_MIN_TESTS &&
             q.tests_passed == q.tests_executed &&
             q.tests_failed == 0 &&
             q.negative_test_executed &&
             q.negative_test_passed;

    printf("Research qualification %u/%u; negative=%d/%d %s\n",
           q.tests_passed, q.tests_executed,
           q.negative_test_passed, q.negative_test_executed,
           ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
