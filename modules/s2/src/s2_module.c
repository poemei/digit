#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "stnlabz_abi/module.h"
#include "s2_model.h"
#include "digit_transport.h"

#define S2_URL "https://api.stn-labz.com/threats"
#define S2_MAX_BYTES (8U * 1024U * 1024U)
#define S2_STATE_DIR "/opt/digit/state/s2"
#define S2_STATE_FILE S2_STATE_DIR "/sentinel-register.txt"
#define S2_POLL_SECONDS 14400U
typedef struct { char *body; size_t length; } received_t;
static pthread_t worker;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static atomic_int stop_requested;
static int thread_running, registered;
static const stnlabz_module_host_t *owner;
static s2_register_t cases;
static char last_result[400] = "No successful Sentinel collection yet.";
static size_t latest_opened, latest_changed, latest_qualifying;
static time_t last_success;
static int initial_report_sent;

/* A successful collection produces at most one short, scheduled IRC synopsis.
 * The first successful poll sends a baseline. Later polls send only on change.
 * Failed delivery remains visible in logs; a subsequent material change can retry. */
static void publish_synopsis(size_t total, size_t opened, size_t changed,
                             size_t qualifying, int baseline)
{
    digit_transport_channel_request_t req = {{0}};
    size_t used = 0;
    stnlabz_module_result_t result;
    if (!owner || !owner->invoke_service) return;
    (void)snprintf(req.message, sizeof req.message,
        "[S2] %s | Sentinel records=%zu; new=%zu revised=%zu; "
        "MEDIUM/HIGH/CRITICAL=%zu. Source records are leads, not confirmed incidents. "
        "Next check in 4h. /s2 for status.",
        baseline ? "Baseline intelligence synopsis" : "Intelligence update",
        total, opened, changed, qualifying);
    result = owner->invoke_service(DIGIT_TRANSPORT_CHANNEL_SEND_SERVICE,
        &req, sizeof req, NULL, 0, &used);
    if (result != STNLABZ_MODULE_OK)
        fprintf(stderr, "[S2] IRC delivery unavailable result=%d; collection retained\\n",
                (int)result);
    else
        fprintf(stderr, "[S2] scheduled synopsis queued to IRC\\n");
}

static size_t on_bytes(char *data, size_t size, size_t amount, void *ctx)
{
    received_t *r = ctx;
    char *next;
    size_t n;
    if (!r || !data || !size || amount > SIZE_MAX / size) return 0;
    n = size * amount;
    if (n > S2_MAX_BYTES - r->length) return 0;
    next = realloc(r->body, r->length + n + 1U);
    if (!next) return 0;
    r->body = next;
    memcpy(r->body + r->length, data, n);
    r->length += n;
    r->body[r->length] = 0;
    return n;
}
static int collect_once(void)
{
    CURL *curl = curl_easy_init();
    received_t data = {0};
    CURLcode result;
    long status = 0;
    s2_register_t next = {0};
    int ok = 0;
    int announce = 0, baseline = 0;
    size_t report_total = 0, report_opened = 0, report_changed = 0, report_qualifying = 0;
    if (!curl) return 0;
    curl_easy_setopt(curl, CURLOPT_URL, S2_URL);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_bytes);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Digit-S2/1.0.2");
    result = curl_easy_perform(curl);
    if (result == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK || status != 200L || !data.body) goto done;
    pthread_mutex_lock(&mutex);
    if (cases.count) {
        next.cases = malloc(cases.count * sizeof *next.cases);
        if (!next.cases) {
            pthread_mutex_unlock(&mutex);
            goto done;
        }
        memcpy(next.cases, cases.cases, cases.count * sizeof *next.cases);
        next.count = cases.count;
        next.capacity = cases.count;
    }
    pthread_mutex_unlock(&mutex);
    if (!s2_ingest_json(&next, data.body, data.length)) goto done;
    if (!s2_save_register(&next, S2_STATE_FILE)) goto done;
    pthread_mutex_lock(&mutex);
    s2_register_free(&cases);
    cases = next;
    memset(&next, 0, sizeof next);
    latest_opened = cases.opened;
    latest_changed = cases.changed;
    latest_qualifying = cases.qualifying;
    last_success = time(NULL);
    baseline = !initial_report_sent;
    announce = baseline || cases.opened != 0U || cases.changed != 0U;
    report_total = cases.count;
    report_opened = cases.opened;
    report_changed = cases.changed;
    report_qualifying = cases.qualifying;
    if (announce) initial_report_sent = 1;
    (void)snprintf(last_result, sizeof last_result,
        "Sentinel source records tracked=%zu; this poll new=%zu revised=%zu unchanged=%zu; "
        "MEDIUM/HIGH/CRITICAL observed=%zu; records without explicit severity=%zu. "
        "Source findings are investigative leads, not proof of compromise.",
        cases.count, cases.opened, cases.changed, cases.unchanged,
        cases.qualifying, cases.missing_severity);
    pthread_mutex_unlock(&mutex);
    fprintf(stderr, "[S2] %s\n", last_result);
    ok = 1;
done:
    if (!ok) {
        /* Failure does not rewrite the register, reset historical cases,
           or misreport an unsuccessful poll as a clean snapshot. */
        fprintf(stderr, "[S2] Sentinel collection failed curl=%d http=%ld\n",
                (int)result, status);
    }
    s2_register_free(&next);
    free(data.body);
    return ok;
}
static void *run_collection(void *unused)
{
    unsigned seconds;
    (void)unused;
    while (!atomic_load(&stop_requested)) {
        (void)collect_once();
        for (seconds = 0; seconds < S2_POLL_SECONDS && !atomic_load(&stop_requested);
             ++seconds) {
            struct timespec wait = {1, 0};
            (void)nanosleep(&wait, NULL);
        }
    }
    return NULL;
}
static stnlabz_module_result_t s2_command(
    const stnlabz_module_command_t *cmd,
    stnlabz_module_command_reply_fn reply, void *reply_context, void *ctx)
{
    char output[480];
    (void)ctx;
    if (!cmd || !reply) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    if (cmd->arguments[0] && strcmp(cmd->arguments, "status"))
        return reply(reply_context, "S2 supports /s2 and /s2 status.")
            ? STNLABZ_MODULE_OK : STNLABZ_MODULE_ERR_INVALID_STATE;
    pthread_mutex_lock(&mutex);
    (void)snprintf(output, sizeof output, "[S2] %s Last successful collection epoch=%lld. "
        "Case counts measure source records, not attacks.",
        last_result, (long long)last_success);
    pthread_mutex_unlock(&mutex);
    return reply(reply_context, output) ? STNLABZ_MODULE_OK :
        STNLABZ_MODULE_ERR_INVALID_STATE;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
    unsigned e = 0, p = 0; int ne = 0, np = 0;
    if (!q) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(q, 0, sizeof *q);
    if (!s2_model_selftest(&e, &p, &ne, &np)) return STNLABZ_MODULE_ERR_QUALIFICATION;
    q->tests_executed=e; q->tests_passed=p; q->tests_failed=e-p;
    q->negative_test_executed=ne; q->negative_test_passed=np;
    return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h)
{
    if (!h || !h->register_command || !h->unregister_command) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    if (mkdir("/opt/digit/state", 0700) && errno != EEXIST)
        return STNLABZ_MODULE_ERR_START_FAILED;
    if (mkdir(S2_STATE_DIR, 0700) && errno != EEXIST)
        return STNLABZ_MODULE_ERR_START_FAILED;
    if (!s2_load_register(&cases, S2_STATE_FILE))
        return STNLABZ_MODULE_ERR_START_FAILED;
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        return STNLABZ_MODULE_ERR_START_FAILED;
    if (!h->register_command("s2", s2_command, NULL)) {
        curl_global_cleanup();
        return STNLABZ_MODULE_ERR_START_FAILED;
    }
    owner = h; registered = 1;
    initial_report_sent = 0;
    atomic_store(&stop_requested, 0);
    if (pthread_create(&worker, NULL, run_collection, NULL)) {
        (void)h->unregister_command("s2", NULL);
        registered = 0; owner = NULL;
        curl_global_cleanup();
        return STNLABZ_MODULE_ERR_START_FAILED;
    }
    thread_running = 1;
    fprintf(stderr, "[S2] module active, collection every %u seconds\n", S2_POLL_SECONDS);
    return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void)
{
    atomic_store(&stop_requested, 1);
    if (thread_running) {
        (void)pthread_join(worker, NULL);
        thread_running = 0;
    }
    if (registered && owner) (void)owner->unregister_command("s2", NULL);
    registered = 0; owner = NULL;
    pthread_mutex_lock(&mutex);
    s2_register_free(&cases);
    pthread_mutex_unlock(&mutex);
    curl_global_cleanup();
    return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor = {
    "s2", "Digit S2 Security Intelligence", 1, 0, 2,
    STNLABZ_MODULE_API_MAJOR, STNLABZ_MODULE_API_MINOR,
    qualify, start, stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{
    return &descriptor;
}
