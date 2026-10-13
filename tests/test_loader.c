#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module_loader.h"
#include "stnlabz_abi/module_registry.h"
static unsigned tests, failed;
static void check(int ok, const char *msg)
{
    ++tests;
    if (!ok) { ++failed; fprintf(stderr, "FAIL loader: %s\n", msg); }
}
int main(int argc, char **argv)
{
    stnlabz_module_loader_t loader;
    stnlabz_module_registry_t registry;
    const stnlabz_module_descriptor_t *d = NULL;
    stnlabz_module_loader_result_t result;
    if (argc != 4) return 2;
    stnlabz_module_loader_init(&loader);
    stnlabz_module_registry_init(&registry);
    check(stnlabz_module_loader_load(&loader, "mock", argv[1], &d) ==
          STNLABZ_MODULE_LOADER_OK, "load valid ABI 1.5 fixture");
    check(d && strcmp(d->id, "mock") == 0, "descriptor identity");
    check(loader.count == 1 && registry.count == 0,
          "loading never discovers, qualifies or activates");
    check(stnlabz_module_loader_load(&loader, "mock", argv[1], &d) ==
          STNLABZ_MODULE_LOADER_ERR_ALREADY_LOADED, "reject duplicate load");
    check(stnlabz_module_loader_unload(&loader, "mock") ==
          STNLABZ_MODULE_LOADER_OK && loader.count == 0, "unload fixture");
    check(stnlabz_module_loader_load(&loader, "wrong", argv[1], &d) ==
          STNLABZ_MODULE_LOADER_ERR_ID_MISMATCH, "reject identity mismatch");
    check(loader.count == 0, "mismatch does not register handle");
    check(stnlabz_module_loader_load(&loader, "mock", argv[2], &d) ==
          STNLABZ_MODULE_LOADER_ERR_EXPORT_MISSING, "reject legacy export");
    check(loader.count == 0, "missing export closes handle");
    check(stnlabz_module_loader_load(&loader, "mock", argv[3], &d) ==
          STNLABZ_MODULE_LOADER_ERR_LOAD_FAILED, "reject missing binary");
    check(stnlabz_module_loader_load(&loader, "", argv[1], &d) ==
          STNLABZ_MODULE_LOADER_ERR_INVALID_ARGUMENT, "reject empty identity");
    result = stnlabz_module_loader_unload(&loader, "mock");
    check(result == STNLABZ_MODULE_LOADER_ERR_NOT_FOUND,
          "reject unload of missing module");
    stnlabz_module_loader_unload_all(&loader);
    check(loader.count == 0 && registry.count == 0, "zero modules retained");
    printf("Core loader tests: %u executed, %u failed\n", tests, failed);
    return failed != 0;
}
