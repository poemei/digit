#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "logging.h"
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
    char directory[] = "/tmp/digit-module-log-XXXXXX";
    char logfile[512], modulefile[512];
    FILE *logread;
    char line[512];
    int saw_register = 0, saw_reject = 0, saw_activate = 0, saw_stop = 0;
    digit_module_manager_t m;
    digit_module_descriptor_t d;
    digit_module_callbacks_t cb = {start_ok, stop_ok, NULL};
    digit_module_qualification_t bad = {1, 1, 0, 0, 0};
    digit_module_qualification_t good = {1, 1, 0, 1, 1};
    if (!mkdtemp(directory)) return 1;
    (void)snprintf(logfile, sizeof logfile, "%s/digit.log", directory);
    (void)snprintf(modulefile, sizeof modulefile, "%s/modules.log", directory);
    digit_module_manager_init(&m);
    memset(&d, 0, sizeof(d));
    strcpy(d.id, "mock");
    d.abi_major = DIGIT_MODULE_ABI_MAJOR;
    check(digit_module_register(&m, &d, &cb) == DIGIT_MODULE_CALLBACK_FAILED,
          "registration denied without audit log");
    check(m.count == 0, "unaudited registration does not change registry");
    if (!digit_log_open_path(logfile)) { rmdir(directory); return 1; }
    digit_module_manager_init(&m);
    check(m.count == 0, "zero modules at initialization");
    memset(&d, 0, sizeof(d));
    strcpy(d.id, "mock");
    d.abi_major = DIGIT_MODULE_ABI_MAJOR;
    check(digit_module_register(&m, &d, &cb) == DIGIT_MODULE_OK, "register mock");
    check(digit_module_register(&m, &d, &cb) == DIGIT_MODULE_DUPLICATE, "reject duplicate");
    check(digit_module_activate(&m, "mock") == DIGIT_MODULE_UNQUALIFIED, "deny activation before qualification");
    check(digit_module_qualify(&m, "mock", &bad) == DIGIT_MODULE_UNQUALIFIED, "negative test required");
    {
        digit_module_qualification_t invalid = good;
        invalid.tests_executed = 0;
        check(digit_module_qualify(&m, "mock", &invalid) == DIGIT_MODULE_UNQUALIFIED,
              "zero tests cannot qualify");
        invalid = good;
        invalid.tests_passed = 0;
        check(digit_module_qualify(&m, "mock", &invalid) == DIGIT_MODULE_UNQUALIFIED,
              "partial pass cannot qualify");
        invalid = good;
        invalid.negative_test_passed = 2;
        check(digit_module_qualify(&m, "mock", &invalid) == DIGIT_MODULE_UNQUALIFIED,
              "nonboolean negative test evidence denied");
        check(digit_module_find(&m, "mock")->state == DIGIT_MODULE_REGISTERED,
              "invalid evidence leaves module unqualified");
    }
    check(digit_module_qualify(&m, "mock", &good) == DIGIT_MODULE_OK, "qualify with evidence");
    check(digit_module_qualify(&m, "mock", &good) == DIGIT_MODULE_UNQUALIFIED,
          "qualification cannot be replayed in qualified state");
    check(digit_module_activate(&m, "mock") == DIGIT_MODULE_UNQUALIFIED, "deny activation without authorization");
    check(digit_module_authorize(&m, "mock", 0) == DIGIT_MODULE_UNQUALIFIED, "deny unauthorized activation");
    check(digit_module_authorize(&m, "mock", 1) == DIGIT_MODULE_OK, "record operator authorization flag");
    check(digit_module_activate(&m, "mock") == DIGIT_MODULE_OK, "activate mock");
    check(starts == 1, "start called");
    check(digit_module_authorize(&m, "mock", 1) == DIGIT_MODULE_UNQUALIFIED,
          "active module cannot be reauthorized");
    check(digit_module_qualify(&m, "mock", &good) == DIGIT_MODULE_UNQUALIFIED,
          "active module cannot be requalified");
    check(digit_module_stop(&m, "mock") == DIGIT_MODULE_OK, "stop mock");
    check(stops == 1, "stop called");
    check(digit_module_activate(&m, "mock") == DIGIT_MODULE_UNQUALIFIED, "re-authorization required after stop");
    check(digit_module_authorize(&m, "mock", 1) == DIGIT_MODULE_OK,
          "stopped qualified module can be reauthorized");
    check(digit_module_activate(&m, "mock") == DIGIT_MODULE_OK,
          "stopped qualified module can be reactivated");
    check(starts == 2, "reactivation starts callback again");
    check(digit_module_stop(&m, "mock") == DIGIT_MODULE_OK,
          "reactivated module can stop");
    check(stops == 2, "second stop callback executed");
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
    check(stops == 2, "no extra stop callback on failed mock");
    check(digit_log_sync(), "module event log synchronized");
    digit_log_close();
    check(digit_module_register(&m, &d, &cb) == DIGIT_MODULE_CALLBACK_FAILED,
          "registration denied once logger closes");
    check(digit_module_authorize(&m, "mock", 1) == DIGIT_MODULE_CALLBACK_FAILED,
          "authorization denied once logger closes");
    logread = fopen(modulefile, "r");
    check(logread != NULL, "module event log readable");
    if (logread) {
        while (fgets(line, sizeof line, logread)) {
            if (strstr(line, "MODULE REGISTER sequence=") && strstr(line, "module=mock")) saw_register = 1;
            if (strstr(line, "MODULE QUALIFY sequence=") && strstr(line, "module=mock")) saw_reject = 1;
            if (strstr(line, "MODULE ACTIVATE sequence=") && strstr(line, "module=mock")) saw_activate = 1;
            if (strstr(line, "MODULE STOP sequence=") && strstr(line, "module=mock")) saw_stop = 1;
        }
        fclose(logread);
    }
    if (!saw_register || !saw_reject || !saw_activate || !saw_stop) {
        FILE *evidence = fopen(modulefile, "r");
        fprintf(stderr, "MODULE LOG EVIDENCE (%s):\n", modulefile);
        if (evidence) {
            while (fgets(line, sizeof line, evidence)) fputs(line, stderr);
            fclose(evidence);
        } else {
            perror("module log evidence unavailable");
        }
    }
    check(saw_register, "registration logged");
    check(saw_reject, "qualification rejection logged");
    check(saw_activate, "activation logged");
    check(saw_stop, "stop logged");
    unlink(logfile);
    unlink(modulefile);
    rmdir(directory);
    printf("Core module-manager tests: %u executed, %u failed\n", tests, failed);
    return failed != 0;
}
