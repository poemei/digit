#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module_loader.h"
static unsigned tests, failed;
static void check(int ok, const char *what) {
    ++tests;
    if (!ok) { ++failed; fprintf(stderr, "FAIL loader growth: %s\n", what); }
}
int main(int argc, char **argv) {
    stnlabz_module_loader_t loader;
    const stnlabz_module_descriptor_t *d = NULL;
    char id[64], path[1024];
    unsigned i;
    if (argc != 2) return 2;
    stnlabz_module_loader_init(&loader);
    for (i = 0; i < 48; ++i) {
        (void)snprintf(id, sizeof id, "grow-%03u", i);
        (void)snprintf(path, sizeof path, "%s/%s.so", argv[1], id);
        check(stnlabz_module_loader_load(&loader, id, path, &d) ==
              STNLABZ_MODULE_LOADER_OK && d && strcmp(d->id,id)==0,
              "load distinct ABI descriptor");
    }
    check(loader.count == 48 && loader.capacity >= 48, "loader exceeds 32");
    check(stnlabz_module_loader_unload(&loader, "grow-003") ==
          STNLABZ_MODULE_LOADER_OK, "unload one handle");
    check(loader.count == 47 && stnlabz_module_loader_find(&loader, "grow-047"),
          "remaining descriptors preserved");
    (void)snprintf(path, sizeof path, "%s/grow-003.so", argv[1]);
    check(stnlabz_module_loader_load(&loader, "grow-003", path, &d) ==
          STNLABZ_MODULE_LOADER_OK && loader.count == 48, "reload after removal");
    stnlabz_module_loader_unload_all(&loader);
    check(loader.count == 0 && loader.capacity == 0 && loader.modules == NULL,
          "all handles and storage released");
    printf("Core loader growth tests: %u executed, %u failed\n", tests, failed);
    return failed != 0;
}
