#include <stdio.h>
#include "stnlabz_abi/module.h"
int main(void)
{
    const stnlabz_module_descriptor_t *m = stnlabz_module_get_descriptor();
    stnlabz_module_qualification_result_t q;
    int ok = m && m->qualify(&q) == STNLABZ_MODULE_OK &&
             q.tests_executed >= 10 && q.tests_passed == q.tests_executed &&
             q.negative_test_executed && q.negative_test_passed;
    printf("Research qualification %u/%u; negative=%d/%d %s\n",
        q.tests_passed,q.tests_executed,q.negative_test_passed,
        q.negative_test_executed,ok?"PASS":"FAIL");
    return ok ? 0 : 1;
}
