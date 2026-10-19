/* [AI:GPT-6 | 2026-10-18] Module contract smoke checks, not qualification. */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "module_loader.h"

int main(int argc, char **argv)
{
    void *handle;
    void *symbol;
    stnlabz_module_get_descriptor_fn get_descriptor;
    const stnlabz_module_descriptor_t *descriptor;
    stnlabz_module_qualification_result_t result = {0};

    if (argc != 2) return 1;
    handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "IRC module load failed: %s\n", dlerror());
        return 1;
    }
    symbol = dlsym(handle, "stnlabz_module_get_descriptor");
    if (!symbol) { dlclose(handle); return 2; }
    memcpy(&get_descriptor, &symbol, sizeof get_descriptor);
    descriptor = get_descriptor();
    if (!descriptor || strcmp(descriptor->id, "irc") ||
        descriptor->required_core_api_major != STNLABZ_MODULE_API_MAJOR ||
        descriptor->required_core_api_minor > STNLABZ_MODULE_API_MINOR ||
        !descriptor->qualify || !descriptor->start || !descriptor->stop) {
        dlclose(handle);
        return 3;
    }
    /* An unfinished suite must not grant qualification or activation. */
    if (descriptor->qualify(&result) == STNLABZ_MODULE_OK) {
        dlclose(handle);
        return 4;
    }
    puts("IRC module smoke test: PASS (descriptor/load/qualification gate)");
    dlclose(handle);
    return 0;
}
/* [End AI:GPT-6] */
