#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "logging.h"

static unsigned tests, failed;
static void check(int ok, const char *name)
{
    ++tests;
    if (!ok) { ++failed; fprintf(stderr, "FAIL logging: %s\n", name); }
}
int main(void)
{
    char path[] = "/tmp/digit-log-XXXXXX";
    char line[512];
    FILE *f;
    int fd = mkstemp(path);
    if (fd < 0) return 1;
    close(fd);
    check(!strcmp(DIGIT_LOG_PATH, "./logs/digit.log"), "canonical path");
    check(!digit_log_event("CORE", "PREOPEN", NULL), "logging before open denied");
    check(!digit_log_open_path("relative"), "relative test path rejected");
    check(digit_log_open_path(path), "private test log opened");
    check(digit_log_is_open(), "log state open");
    check(!digit_log_open_path(path), "second open denied");
    check(digit_log_event("CORE", "START", "test=true"), "Core event recorded");
    check(digit_log_event("MODULE", "REJECTED", "reason=unqualified"), "module event recorded");
    check(digit_log_sync(), "synchronized");
    digit_log_close();
    check(!digit_log_is_open(), "log closed");
    f = fopen(path, "r");
    check(f != NULL, "test log readable");
    if (f) {
        check(fgets(line, sizeof line, f) != NULL && strstr(line, "CORE START test=true") != NULL,
              "Core entry present");
        check(fgets(line, sizeof line, f) != NULL && strstr(line, "MODULE REJECTED reason=unqualified") != NULL,
              "module entry present");
        fclose(f);
    }
    unlink(path);
    printf("Core logging tests: %u executed, %u failed\n", tests, failed);
    return failed != 0;
}
