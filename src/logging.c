#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "logging.h"

static FILE *digit_log_stream;

int digit_log_is_open(void) { return digit_log_stream != NULL; }

int digit_log_open_path(const char *path)
{
    if (digit_log_stream) return 0;
    if (!path || path[0] != '/') return 0;
    digit_log_stream = fopen(path, "a");
    if (!digit_log_stream) return 0;
    (void)setvbuf(digit_log_stream, NULL, _IOLBF, 0);
    return 1;
}

int digit_log_open(void)
{
    if (mkdir(DIGIT_LOG_DIRECTORY, 0700) != 0 && errno != EEXIST) return 0;
    if (digit_log_stream) return 0;
    digit_log_stream = fopen(DIGIT_LOG_PATH, "a");
    if (!digit_log_stream) return 0;
    (void)setvbuf(digit_log_stream, NULL, _IOLBF, 0);
    return 1;
}

int digit_log_event(const char *component, const char *event, const char *detail)
{
    time_t now;
    struct tm utc;
    char stamp[32];
    int written;
    if (!digit_log_stream || !component || !event) return 0;
    now = time(NULL);
    if (now == (time_t)-1 || !gmtime_r(&now, &utc) ||
        !strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%SZ", &utc)) return 0;
    written = fprintf(digit_log_stream, "%s %s %s%s%s\n",
        stamp, component, event,
        detail && *detail ? " " : "", detail && *detail ? detail : "");
    return written > 0 && !ferror(digit_log_stream);
}

int digit_log_sync(void)
{
    return digit_log_stream && fflush(digit_log_stream) == 0 &&
        fsync(fileno(digit_log_stream)) == 0;
}

void digit_log_close(void)
{
    if (!digit_log_stream) return;
    (void)fflush(digit_log_stream);
    (void)fclose(digit_log_stream);
    digit_log_stream = NULL;
}
