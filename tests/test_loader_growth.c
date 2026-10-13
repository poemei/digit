#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stnlabz_abi/module_loader.h"
static unsigned tests, failed;
static int deny_realloc;
void *__real_realloc(void *, size_t);
void *__wrap_realloc(void *ptr, size_t size) {
    if (deny_realloc) return NULL;
    return __real_realloc(ptr, size);
}
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
    for (i = 0; i < 64; ++i) {
        (void)snprintf(id, sizeof id, "grow-%03u", i);
        (void)snprintf(path, sizeof path, "%s/%s.so", argv[1], id);
        check(stnlabz_module_loader_load(&loader, id, path, &d) ==
              STNLABZ_MODULE_LOADER_OK && d && strcmp(d->id,id)==0,
              "load distinct ABI descriptor");
    }
    check(loader.count == 64 && loader.capacity >= 64, "loader exceeds 32");
    (void)snprintf(path, sizeof path, "%s/grow-064.so", argv[1]);
    deny_realloc = 1;
    check(stnlabz_module_loader_load(&loader, "grow-064", path, &d) ==
          STNLABZ_MODULE_LOADER_ERR_FULL, "allocation failure denies new load");
    check(loader.count == 64 && stnlabz_module_loader_find(&loader, "grow-047"),
          "failed growth preserves loaded modules");
    deny_realloc = 0;
    check(stnlabz_module_loader_load(&loader, "grow-064", path, &d) ==
          STNLABZ_MODULE_LOADER_OK, "loader recovers on retry");
    check(stnlabz_module_loader_unload(&loader, "grow-003") ==
          STNLABZ_MODULE_LOADER_OK, "unload one handle");
    check(loader.count == 64 && stnlabz_module_loader_find(&loader, "grow-047"),
          "remaining descriptors preserved");
    (void)snprintf(path, sizeof path, "%s/grow-003.so", argv[1]);
    check(stnlabz_module_loader_load(&loader, "grow-003", path, &d) ==
          STNLABZ_MODULE_LOADER_OK && loader.count == 65, "reload after removal");
    stnlabz_module_loader_unload_all(&loader);
    check(loader.count == 0 && loader.capacity == 0 && loader.modules == NULL,
          "all handles and storage released");
    printf("Core loader growth tests: %u executed, %u failed\n", tests, failed);
    return failed != 0;
}
