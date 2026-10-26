#define _POSIX_C_SOURCE 200809L

#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "stnlabz_abi/module.h"
#include "intelligence_internal.h"

#define INTELLIGENCE_VERSION_MAJOR 1U
#define INTELLIGENCE_VERSION_MINOR 0U
#define INTELLIGENCE_VERSION_PATCH 5U
#define INTELLIGENCE_RESPONSE_MAX (4U * 1024U * 1024U)
#define INTELLIGENCE_RECORD_MAX 512U
#define INTELLIGENCE_FEED_URL "https://api.stn-labz.com/threats"
#define INTELLIGENCE_INTERVAL_SECONDS 60U
#define INTELLIGENCE_STATE_DIRECTORY "/opt/rictus/state"

typedef struct {
    char *data;
    size_t used;
    size_t capacity;
} response_buffer_t;

static pthread_t worker;
static atomic_int stop_requested;
static int worker_started;

static int bounded_text(const char *text)
{
    const unsigned char *p = (const unsigned char *)text;
    if (!text || !*text) return 0;
    while (*p) {
        if (*p < 0x20U || *p == 0x7fU) return 0;
        ++p;
    }
    return 1;
}

static int write_all(int fd, const char *data, size_t length)
{
    size_t offset = 0U;
    while (offset < length) {
        ssize_t written = write(fd, data + offset, length - offset);
        if (written < 0) {
            if (errno == EINTR) continue;
            return 0;
        }
        if (!written) return 0;
        offset += (size_t)written;
    }
    return 1;
}

static int log_line(const char *format, ...)
{
    char message[1024];
    char line[1200];
    char stamp[32];
    time_t now;
    struct tm utc;
    va_list arguments;
    int message_length;
    int line_length;
    int fd;

    va_start(arguments, format);
    message_length = vsnprintf(message, sizeof message, format, arguments);
    va_end(arguments);
    if (message_length < 0 || (size_t)message_length >= sizeof message) return 0;

    now = time(NULL);
    if (now == (time_t)-1 || !gmtime_r(&now, &utc) ||
        !strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%SZ", &utc))
        return 0;

    line_length = snprintf(line, sizeof line, "%s INTELLIGENCE %s\n", stamp, message);
    if (line_length < 0 || (size_t)line_length >= sizeof line) return 0;

    fd = open(RICTUS_INTELLIGENCE_LOG_PATH,
              O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0640);
    if (fd < 0) return 0;
    if (!write_all(fd, line, (size_t)line_length)) {
        close(fd);
        return 0;
    }
    if (fsync(fd) != 0) {
        close(fd);
        return 0;
    }
    return close(fd) == 0;
}

static int sleep_interruptible(unsigned int seconds)
{
    struct timespec delay = {1, 0};
    unsigned int i;

    for (i = 0U; i < seconds; ++i) {
        struct timespec remaining = delay;
        if (atomic_load(&stop_requested)) return 0;
        while (nanosleep(&remaining, &remaining) != 0) {
            if (errno == EINTR) {
                if (atomic_load(&stop_requested)) return 0;
                continue;
            }
            return 0;
        }
    }
    return !atomic_load(&stop_requested);
}

static size_t response_write(void *contents, size_t size, size_t nmemb, void *context)
{
    response_buffer_t *buffer = context;
    size_t bytes;
    size_t required;
    char *next;

    if (!buffer || !contents || size == 0U || nmemb == 0U) return 0U;
    if (nmemb > SIZE_MAX / size) return 0U;
    bytes = size * nmemb;
    if (buffer->used > INTELLIGENCE_RESPONSE_MAX ||
        bytes > INTELLIGENCE_RESPONSE_MAX - buffer->used)
        return 0U;

    required = buffer->used + bytes + 1U;
    if (required > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 4096U;
        while (capacity < required) {
            if (capacity >= INTELLIGENCE_RESPONSE_MAX / 2U) {
                capacity = INTELLIGENCE_RESPONSE_MAX + 1U;
                break;
            }
            capacity *= 2U;
        }
        if (capacity > INTELLIGENCE_RESPONSE_MAX + 1U) return 0U;
        next = realloc(buffer->data, capacity);
        if (!next) return 0U;
        buffer->data = next;
        buffer->capacity = capacity;
    }

    memcpy(buffer->data + buffer->used, contents, bytes);
    buffer->used += bytes;
    buffer->data[buffer->used] = '\0';
    return bytes;
}

static int fetch_feed(char **json)
{
    CURL *curl;
    CURLcode result;
    long status = 0L;
    response_buffer_t response = {0};

    if (!json) return 0;
    *json = NULL;

    curl = curl_easy_init();
    if (!curl) return 0;

    curl_easy_setopt(curl, CURLOPT_URL, INTELLIGENCE_FEED_URL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "STN-LABZ-Rictus-Intelligence/1.0");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    result = curl_easy_perform(curl);
    if (result == CURLE_OK)
        (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK || status != 200L || !response.data || !response.used) {
        free(response.data);
        return 0;
    }

    *json = response.data;
    return 1;
}

static int ensure_state_directory(void)
{
    struct stat status;

    if (stat(INTELLIGENCE_STATE_DIRECTORY, &status) == 0)
        return S_ISDIR(status.st_mode);

    if (errno != ENOENT) return 0;
    if (mkdir(INTELLIGENCE_STATE_DIRECTORY, 0750) != 0 && errno != EEXIST)
        return 0;

    return stat(INTELLIGENCE_STATE_DIRECTORY, &status) == 0 &&
           S_ISDIR(status.st_mode);
}

static int read_cursor(char *cursor, size_t cursor_size)
{
    FILE *file;
    size_t length;

    if (!cursor || cursor_size < 2U) return 0;
    cursor[0] = '\0';

    if (!ensure_state_directory()) return 0;
    file = fopen(RICTUS_INTELLIGENCE_STATE_PATH, "r");
    if (!file) return errno == ENOENT ? 1 : 0;
    if (!fgets(cursor, (int)cursor_size, file)) {
        fclose(file);
        return 0;
    }
    if (fclose(file) != 0) return 0;

    length = strlen(cursor);
    while (length && (cursor[length - 1U] == '\n' || cursor[length - 1U] == '\r'))
        cursor[--length] = '\0';
    return length == 0U || bounded_text(cursor);
}

static int save_cursor(const char *cursor)
{
    char temporary[512];
    int fd;
    int length;

    if (!cursor || !bounded_text(cursor)) return 0;
    if (!ensure_state_directory()) return 0;
    length = snprintf(temporary, sizeof temporary, "%s.tmp", RICTUS_INTELLIGENCE_STATE_PATH);
    if (length < 0 || (size_t)length >= sizeof temporary) return 0;

    fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0640);
    if (fd < 0) return 0;
    if (!write_all(fd, cursor, strlen(cursor)) ||
        !write_all(fd, "\n", 1U) ||
        fsync(fd) != 0) {
        close(fd);
        unlink(temporary);
        return 0;
    }
    if (close(fd) != 0) {
        unlink(temporary);
        return 0;
    }
    if (rename(temporary, RICTUS_INTELLIGENCE_STATE_PATH) != 0) {
        unlink(temporary);
        return 0;
    }
    return 1;
}

static int process_feed(const char *json, char *cursor, size_t cursor_size)
{
    rictus_intelligence_record_t records[INTELLIGENCE_RECORD_MAX];
    size_t count = 0U;
    size_t cursor_index = 0U;
    size_t i;
    int cursor_found = 0;

    if (!rictus_intelligence_extract_records(
            json, records, INTELLIGENCE_RECORD_MAX, &count))
        return log_line("PARSE_FAILED");

    if (!count) return 1;

    if (!cursor[0]) {
        if (!save_cursor(records[0].id)) return log_line("STATE_SAVE_FAILED");
        (void)snprintf(cursor, cursor_size, "%s", records[0].id);
        return log_line("BASELINE cursor=%s records=%zu", records[0].id, count);
    }

    if (!strcmp(cursor, records[0].id)) return 1;

    for (i = 0U; i < count; ++i) {
        if (!strcmp(cursor, records[i].id)) {
            cursor_found = 1;
            cursor_index = i;
            break;
        }
    }

    if (!cursor_found) {
        if (!save_cursor(records[0].id)) return log_line("STATE_SAVE_FAILED");
        (void)snprintf(cursor, cursor_size, "%s", records[0].id);
        return log_line("CURSOR_GAP reset=%s records=%zu", records[0].id, count);
    }

    for (i = cursor_index; i > 0U; --i) {
        const rictus_intelligence_record_t *record = &records[i - 1U];

        if (!rictus_intelligence_relevant(record->relevance)) continue;
        if (!bounded_text(record->id) || !bounded_text(record->type) ||
            !bounded_text(record->severity) || !bounded_text(record->relevance))
            continue;

        if (!log_line("CANDIDATE id=%s type=%s severity=%s relevance=%s observed=%s",
                      record->id,
                      record->type,
                      record->severity,
                      record->relevance,
                      record->created_at[0] ? record->created_at : "unknown"))
            return 0;
    }

    if (!save_cursor(records[0].id)) return log_line("STATE_SAVE_FAILED");
    (void)snprintf(cursor, cursor_size, "%s", records[0].id);
    return 1;
}

static void *intelligence_worker(void *unused)
{
    char cursor[RICTUS_INTELLIGENCE_ID_MAX];
    (void)unused;

    if (!read_cursor(cursor, sizeof cursor)) {
        (void)log_line("STATE_READ_FAILED");
        cursor[0] = '\0';
    }

    (void)log_line("WATCH_STARTED source=%s interval=%u",
                   INTELLIGENCE_FEED_URL,
                   INTELLIGENCE_INTERVAL_SECONDS);

    while (!atomic_load(&stop_requested)) {
        char *json = NULL;

        if (!fetch_feed(&json)) {
            (void)log_line("FETCH_FAILED source=%s", INTELLIGENCE_FEED_URL);
        } else {
            (void)process_feed(json, cursor, sizeof cursor);
            free(json);
        }

        if (!sleep_interruptible(INTELLIGENCE_INTERVAL_SECONDS)) break;
    }

    (void)log_line("WATCH_STOPPED");
    return NULL;
}

static stnlabz_module_result_t intelligence_qualify(
    stnlabz_module_qualification_result_t *result)
{
    unsigned int executed = 0U;
    unsigned int passed = 0U;
    int negative_executed = 0;
    int negative_passed = 0;
    int ok;

    if (!result) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(result, 0, sizeof *result);

    ok = rictus_intelligence_run_selftests(
        &executed, &passed, &negative_executed, &negative_passed);

    result->tests_executed = executed;
    result->tests_passed = passed;
    result->tests_failed = executed - passed;
    result->negative_test_executed = negative_executed;
    result->negative_test_passed = negative_passed;

    return ok ? STNLABZ_MODULE_OK : STNLABZ_MODULE_ERR_QUALIFICATION;
}

static stnlabz_module_result_t intelligence_start(const stnlabz_module_host_t *host)
{
    (void)host;

    if (worker_started) return STNLABZ_MODULE_ERR_INVALID_STATE;

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fprintf(stderr, "[INTELLIGENCE] libcurl initialization failed\n");
        (void)log_line("START_FAILED reason=curl_global_init");
        return STNLABZ_MODULE_ERR_START_FAILED;
    }

    atomic_store(&stop_requested, 0);

    if (pthread_create(&worker, NULL, intelligence_worker, NULL) != 0) {
        fprintf(stderr, "[INTELLIGENCE] worker thread creation failed\n");
        (void)log_line("START_FAILED reason=worker_create");
        curl_global_cleanup();
        return STNLABZ_MODULE_ERR_START_FAILED;
    }

    worker_started = 1;
    fprintf(stderr, "[INTELLIGENCE] watcher active\n");
    return STNLABZ_MODULE_OK;
}

static stnlabz_module_result_t intelligence_stop(void)
{
    if (!worker_started) return STNLABZ_MODULE_ERR_INVALID_STATE;

    atomic_store(&stop_requested, 1);
    if (pthread_join(worker, NULL) != 0)
        return STNLABZ_MODULE_ERR_STOP_FAILED;

    worker_started = 0;
    curl_global_cleanup();
    return STNLABZ_MODULE_OK;
}

static const stnlabz_module_descriptor_t descriptor = {
    "intelligence",
    "Rictus Intelligence",
    INTELLIGENCE_VERSION_MAJOR,
    INTELLIGENCE_VERSION_MINOR,
    INTELLIGENCE_VERSION_PATCH,
    STNLABZ_MODULE_API_MAJOR,
    STNLABZ_MODULE_API_MINOR,
    intelligence_qualify,
    intelligence_start,
    intelligence_stop
};

const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{
    return &descriptor;
}
