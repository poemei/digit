#include <stdio.h>
#include "agent_root.h"
#include "digit.h"
#include "runtime.h"

/* Core-only entry point. No module discovery, Dispatcher or transport dependency. */
int digit_initialize(void)
{
    digit_runtime_t runtime;
    const stnlabz_agent_root_t *root = stnlabz_agent_root_identity();
    if (!stnlabz_agent_root_validate(root)) {
        fputs("[CORE] Agent Root validation failed. Startup denied.\n", stderr);
        return 1;
    }
    digit_runtime_init(&runtime);
    printf("%s\n", DIGIT_NAME);
    printf("[CORE] Agent Root verified: %s (revision %u)\n",
           root->agent_name, (unsigned)root->revision);
    puts("[CORE] Running without external modules. SIGINT/SIGTERM to stop.");
    fflush(stdout);
    if (!digit_runtime_run(&runtime)) {
        fputs("[CORE] Runtime failed.\n", stderr);
        return 1;
    }
    puts("[CORE] Stopped.");
    return 0;
}
