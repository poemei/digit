#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stnlabz_abi/module_registry.h"
static unsigned tests, failed;
static int deny_realloc;
void *__real_realloc(void *, size_t);
void *__wrap_realloc(void *ptr, size_t size)
{
    if (deny_realloc) return NULL;
    return __real_realloc(ptr, size);
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
    memset(q, 0, sizeof *q);
    q->tests_executed = q->tests_passed = STNLABZ_MODULE_MIN_TESTS;
    q->negative_test_executed = q->negative_test_passed = 1;
    return STNLABZ_MODULE_OK;
}
static void check(int ok, const char *message)
{
    ++tests;
    if (!ok) { ++failed; fprintf(stderr, "FAIL registry growth: %s\n", message); }
}
int main(void)
{
    stnlabz_module_registry_t r;
    stnlabz_module_descriptor_t d;
    const stnlabz_module_record_t *record;
    unsigned i;
    stnlabz_module_registry_init(&r);
    memset(&d, 0, sizeof d);
    strcpy(d.name, "Registry fixture");
    d.required_core_api_major = STNLABZ_MODULE_API_MAJOR;
    d.required_core_api_minor = STNLABZ_MODULE_API_MINOR;
    d.qualify = qualify;
    for (i = 0; i < 128; ++i) {
        (void)snprintf(d.id, sizeof d.id, "grow-%03u", i);
        check(stnlabz_module_registry_discover(&r, &d) == STNLABZ_MODULE_OK,
              "register without 32-module ceiling");
    }
    check(r.count == 128 && r.capacity >= 128, "growth above 32");
    check(stnlabz_module_registry_discover(&r, &d) == STNLABZ_MODULE_ERR_DUPLICATE,
          "duplicate identity denied after expansion");
    check(stnlabz_module_registry_verify(&r, "grow-011") == STNLABZ_MODULE_OK,
          "verify preserved record");
    check(stnlabz_module_registry_qualify(&r, "grow-011") == STNLABZ_MODULE_OK,
          "qualification state survives growth");
    record = stnlabz_module_registry_find(&r, "grow-011");
    check(record && record->state == STNLABZ_MODULE_STATE_QUALIFIED &&
          record->qualification.tests_passed == STNLABZ_MODULE_MIN_TESTS &&
          !record->activation_authorized, "qualification evidence and gate retained");
    check(stnlabz_module_registry_unregister(&r, "grow-011") == STNLABZ_MODULE_OK,
          "remove qualified record");
    check(r.count == 127 && !stnlabz_module_registry_find(&r, "grow-011"),
          "compaction after removal");
    (void)snprintf(d.id, sizeof d.id, "reused-slot");
    check(stnlabz_module_registry_discover(&r, &d) == STNLABZ_MODULE_OK &&
          r.count == 128, "reuse removed slot");
    deny_realloc = 1;
    for (i = 128; i < 256; ++i) {
        (void)snprintf(d.id, sizeof d.id, "grow-%03u", i);
        if (r.count == r.capacity) break;
        check(stnlabz_module_registry_discover(&r, &d) == STNLABZ_MODULE_OK,
              "fill existing capacity");
    }
    (void)snprintf(d.id, sizeof d.id, "allocation-fails");
    check(stnlabz_module_registry_discover(&r, &d) ==
          STNLABZ_MODULE_ERR_REGISTRY_FULL, "allocation failure reported safely");
    check(r.count == r.capacity &&
          stnlabz_module_registry_find(&r, "reused-slot") != NULL,
          "failure preserves records");
    deny_realloc = 0;
    check(stnlabz_module_registry_discover(&r, &d) == STNLABZ_MODULE_OK,
          "retry succeeds after allocation recovery");
    stnlabz_module_registry_destroy(&r);
    check(r.modules == NULL && r.count == 0 && r.capacity == 0,
          "registry heap released");
    printf("Core registry growth tests: %u executed, %u failed\n", tests, failed);
    return failed != 0;
}
