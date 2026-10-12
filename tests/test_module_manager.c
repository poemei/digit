#include <stdio.h>
#include <string.h>
#include "module_manager.h"
static unsigned tests, failed;
static int starts, stops;
static int start_ok(void *unused) { (void)unused; ++starts; return 1; }
static int start_fail(void *unused) { (void)unused; return 0; }
static void stop_ok(void *unused) { (void)unused; ++stops; }
static void check(int result, const char *name) {
    ++tests;
    if (!result) { ++failed; fprintf(stderr, "FAIL module management: %s\n", name); }
}
int main(void) {
    digit_module_manager_t m;
    digit_module_descriptor_t d;
    digit_module_callbacks_t cb = {start_ok, stop_ok, NULL};
    digit_module_qualification_t bad = {1, 1, 0, 0, 0};
    digit_module_qualification_t good = {1, 1, 0, 1, 1};
    digit_module_manager_init(&m);
    check(m.count == 0, "zero modules at initialization");
    memset(&d, 0, sizeof(d));
    strcpy(d.id, "mock");
    d.abi_major = DIGIT_MODULE_ABI_MAJOR;
    check(digit_module_register(&m, &d, &cb) == DIGIT_MODULE_OK, "register mock");
    check(digit_module_register(&m, &d, &cb) == DIGIT_MODULE_DUPLICATE, "reject duplicate");
    check(digit_module_activate(&m, "mock") == DIGIT_MODULE_UNQUALIFIED, "deny activation before qualification");
    check(digit_module_qualify(&m, "mock", &bad) == DIGIT_MODULE_UNQUALIFIED, "negative test required");
    check(digit_module_qualify(&m, "mock", &good) == DIGIT_MODULE_OK, "qualify with evidence");
    check(digit_module_activate(&m, "mock") == DIGIT_MODULE_UNQUALIFIED, "deny activation without authorization");
    check(digit_module_authorize(&m, "mock", 0) == DIGIT_MODULE_UNQUALIFIED, "deny unauthorized activation");
    check(digit_module_authorize(&m, "mock", 1) == DIGIT_MODULE_OK, "record operator authorization flag");
    check(digit_module_activate(&m, "mock") == DIGIT_MODULE_OK, "activate mock");
    check(starts == 1, "start called");
    check(digit_module_stop(&m, "mock") == DIGIT_MODULE_OK, "stop mock");
    check(stops == 1, "stop called");
    check(digit_module_activate(&m, "mock") == DIGIT_MODULE_UNQUALIFIED, "re-authorization required after stop");
    d.abi_major++;
    strcpy(d.id, "incompatible");
    check(digit_module_register(&m, &d, &cb) == DIGIT_MODULE_INVALID, "reject ABI mismatch");
    d.abi_major = DIGIT_MODULE_ABI_MAJOR;
    strcpy(d.id, "failed-start");
    cb.start = start_fail;
    check(digit_module_register(&m, &d, &cb) == DIGIT_MODULE_OK, "register failing mock");
    check(digit_module_qualify(&m, "failed-start", &good) == DIGIT_MODULE_OK, "qualify failing mock");
    check(digit_module_authorize(&m, "failed-start", 1) == DIGIT_MODULE_OK, "authorize failing mock");
    check(digit_module_activate(&m, "failed-start") == DIGIT_MODULE_CALLBACK_FAILED, "contain callback failure");
    check(digit_module_find(&m, "failed-start")->state != DIGIT_MODULE_ACTIVE, "failed mock not active");
    digit_module_manager_shutdown(&m);
    check(stops == 1, "no extra stop callback on failed mock");
    printf("Core module-manager tests: %u executed, %u failed\n", tests, failed);
    return failed != 0;
}
