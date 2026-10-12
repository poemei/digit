#include <stdio.h>
#include "agent_root.h"
#include "digit.h"
#include "module_manager.h"
#include "runtime.h"

/* Core-only entry point. No module discovery, Dispatcher or transport dependency. */
int digit_initialize(void)
{
    digit_runtime_t runtime;
    digit_module_manager_t modules;
    const stnlabz_agent_root_t *root = stnlabz_agent_root_identity();
    if (!stnlabz_agent_root_validate(root)) {
        fputs("[CORE] Agent Root validation failed. Startup denied.\n", stderr);
        return 1;
    }
    digit_runtime_init(&runtime);
    digit_module_manager_init(&modules);
    printf("%s\n", DIGIT_NAME);
    printf("[CORE] Agent Root verified: %s (revision %u)\n",
           root->agent_name, (unsigned)root->revision);
    printf("[CORE] Module manager ready: %zu registered modules.\n", modules.count);
    puts("[CORE] Running without external modules. SIGINT/SIGTERM to stop.");
    fflush(stdout);
    if (!digit_runtime_run(&runtime)) {
        digit_module_manager_shutdown(&modules);
        fputs("[CORE] Runtime failed.\n", stderr);
        return 1;
    }
    digit_module_manager_shutdown(&modules);
    puts("[CORE] Stopped.");
    return 0;
}
