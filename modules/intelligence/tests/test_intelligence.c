#include <stdio.h>
#include "intelligence_internal.h"

int main(void)
{
    unsigned int executed = 0U;
    unsigned int passed = 0U;
    int negative_executed = 0;
    int negative_passed = 0;
    int ok = rictus_intelligence_run_selftests(
        &executed, &passed, &negative_executed, &negative_passed);

    printf("intelligence tests: executed=%u passed=%u failed=%u negative_executed=%d negative_passed=%d\n",
           executed, passed, executed - passed,
           negative_executed, negative_passed);
    return ok ? 0 : 1;
}
